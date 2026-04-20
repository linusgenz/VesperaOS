/*
** lvespera.c - VesperaOS system module for Lua
**
** Provides Lua bindings for VesperaOS syscalls and system operations.
** This module enables Lua scripts to interact with the kernel for:
** - Process management (spawn, exit, wait)
** - File I/O (open, read, write, close)
** - Filesystem operations (mkdir, unlink, chdir, getcwd)
** - System operations (sleep, reboot, mount)
** - Logging and debugging
*/

#define lvespera_c
#define LUA_LIB

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

/* VesperaOS headers */
#include <dirent.h>
#include <fflags.h>
#include <file.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysstd.h>
#include <time.h>
#include <vespera/handles.h>
#include <vespera/poll.h>
#include <vespera/stat.h>

/* Handle type for Lua userdata */
#define VESPERA_HANDLE_META "vespera_handle"

static int push_result(lua_State *L, int64_t result) {
    if (result < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, (lua_Integer)result);
        return 2;
    }
    lua_pushinteger(L, (lua_Integer)result);
    return 1;
}

static int push_success(lua_State *L) {
    lua_pushboolean(L, 1);
    return 1;
}

/* ============================================================================
** Process Management
** ============================================================================ */

/*
** vespera.spawn(path, [args], [env])
**
** Spawns a new process (realm) with the given executable path.
**
** @param path Path to executable
** @param args Optional table of command-line arguments
** @param env  Optional table of environment variables
** @return Unit handle on success, or nil + error code on failure
*/
static int lvespera_spawn(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);

    /* For now, simple spawn without args/env */
    int64_t result = sys_spawn((uint64_t)path, 0, 0, 0, 0, 0);

    if (result > 0) {
        lua_pushinteger(L, (lua_Integer)result);
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.exit([code])
**
** Terminate the current unit.
**
** @param code Optional exit code (default: 0)
** @return Does not return
*/
static int lvespera_exit(lua_State *L) {
    int code = (int)luaL_optinteger(L, 1, 0);
    sys_exit((uint64_t)code, 0, 0, 0, 0, 0);
    /* NOTREACHED */
    return 0;
}

/*
** vespera.wait(handle)
**
** Wait for a realm to terminate.
**
** @param handle Realm handle to wait for
** @return Exit code on success, or nil + error code
*/
static int lvespera_wait(lua_State *L) {
    uint64_t realm = (uint64_t)luaL_checkinteger(L, 1);
    int exit_code = 0;

    int64_t result = sys_wait(realm, (uint64_t)&exit_code, 0, 0, 0, 0);

    if (result == 0) {
        lua_pushinteger(L, exit_code);
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.kill(handle, [signal])
**
** Send a signal to a unit/realm.
**
** @param handle Target handle
** @param signal Signal number (default: 9/SIGKILL)
** @return true on success, or nil + error code
*/
static int lvespera_kill(lua_State *L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    int sig = (int)luaL_optinteger(L, 2, 9);

    int64_t result = sys_kill(handle, (uint64_t)sig, 0, 0, 0, 0);

    if (result == 0) {
        return push_success(L);
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.getrid()
**
** Get the current realm and unit IDs.
**
** @return realm_id, unit_id
*/
static int lvespera_getrid(lua_State *L) {
    uint64_t realm_id = 0, unit_id = 0;

    sys_getrid((uint64_t)&realm_id, (uint64_t)&unit_id, 0, 0, 0, 0);

    lua_pushinteger(L, (lua_Integer)realm_id);
    lua_pushinteger(L, (lua_Integer)unit_id);
    return 2;
}

/* ============================================================================
** File I/O
** ============================================================================ */

/*
** vespera.open(path, mode)
**
** Open a file.
**
** @param path File path
** @param mode Open mode ("r", "w", "rw", etc.)
** @return File handle on success, or nil + error code
*/
static int lvespera_open(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    const char *mode = luaL_optstring(L, 2, "r");

    int flags = 0;
    if (strchr(mode, 'w') && strchr(mode, '+')) {
        flags = 0x02 | 0x01; /* O_WRONLY | O_RDWR */
    } else if (strchr(mode, 'w')) {
        flags = 0x02; /* O_WRONLY */
    } else if (strchr(mode, '+')) {
        flags = 0x01 | 0x02; /* O_RDWR */
    } else {
        flags = 0x01; /* O_RDONLY */
    }

    int64_t handle = sys_open((uint64_t)path, (uint64_t)flags, 0, 0, 0, 0);

    if (handle < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, handle);
        return 2;
    }

    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

/*
** vespera.close(handle)
**
** Close a file handle.
**
** @param handle File handle
** @return true on success, or nil + error code
*/
static int lvespera_close(lua_State *L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);

    int64_t result = sys_close(handle, 0, 0, 0, 0, 0);

    if (result == 0) {
        return push_success(L);
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.read(handle, count)
**
** Read data from a file handle.
**
** @param handle File handle
** @param count Maximum bytes to read
** @return Data string on success, or nil + error code
*/
static int lvespera_read(lua_State *L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    size_t count = (size_t)luaL_checkinteger(L, 2);

    char *buf = (char *)malloc(count);
    if (!buf) {
        lua_pushnil(L);
        lua_pushliteral(L, "out of memory");
        return 2;
    }

    int64_t result = sys_read(handle, (uint64_t)buf, count, 0, 0, 0);

    if (result < 0) {
        free(buf);
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }

    lua_pushlstring(L, buf, (size_t)result);
    free(buf);
    return 1;
}

/*
** vespera.write(handle, data)
**
** Write data to a file handle.
**
** @param handle File handle
** @param data String to write
** @return Bytes written on success, or nil + error code
*/
static int lvespera_write(lua_State *L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);

    int64_t result = sys_write(handle, (uint64_t)data, len, 0, 0, 0);

    if (result < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }

    lua_pushinteger(L, result);
    return 1;
}

/*
** vespera.seek(handle, offset, whence)
**
** Seek to a position in a file.
**
** @param handle File handle
** @param offset Byte offset
** @param whence SEEK_SET, SEEK_CUR, or SEEK_END
** @return New position on success, or nil + error code
*/
static int lvespera_seek(lua_State *L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    int64_t offset = (int64_t)luaL_checkinteger(L, 2);
    int64_t whence = (int64_t)luaL_optinteger(L, 3, SEEK_SET);

    int64_t result = sys_seek(handle, (uint64_t)offset, (uint64_t)whence, 0, 0, 0);

    if (result < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }

    lua_pushinteger(L, (lua_Integer)result);
    return 1;
}

/* ============================================================================
** Filesystem Operations
** ============================================================================ */

/*
** vespera.mkdir(path)
**
** Create a directory.
**
** @param path Directory path
** @return true on success, or nil + error code
*/
static int lvespera_mkdir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    int64_t result = sys_mkdir((uint64_t)path, 0, 0, 0, 0, 0);

    if (result == 0) {
        return push_success(L);
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.unlink(path)
**
** Remove a file.
**
** @param path File path
** @return true on success, or nil + error code
*/
static int lvespera_unlink(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    int64_t result = sys_unlink((uint64_t)path, 0, 0, 0, 0, 0);

    if (result == 0) {
        return push_success(L);
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.rmdir(path)
**
** Remove a directory.
**
** @param path Directory path
** @return true on success, or nil + error code
*/
static int lvespera_rmdir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    int64_t result = sys_rmdir((uint64_t)path, 0, 0, 0, 0, 0);

    if (result == 0) {
        return push_success(L);
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.rename(oldpath, newpath)
**
** Rename a file or directory.
**
** @param oldpath Old path
** @param newpath New path
** @return true on success, or nil + error code
*/
static int lvespera_rename(lua_State *L) {
    const char *oldpath = luaL_checkstring(L, 1);
    const char *newpath = luaL_checkstring(L, 2);
    int64_t result = sys_rename((uint64_t)oldpath, (uint64_t)newpath, 0, 0, 0, 0);

    if (result == 0) {
        return push_success(L);
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.chdir(path)
**
** Change current working directory.
**
** @param path Directory path
** @return true on success, or nil + error code
*/
static int lvespera_chdir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    int64_t result = sys_chdir((uint64_t)path, 0, 0, 0, 0, 0);

    if (result == 0) {
        return push_success(L);
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.getcwd()
**
** Get current working directory.
**
** @return Path string on success, or nil + error code
*/
static int lvespera_getcwd(lua_State *L) {
    char buf[1024];
    int64_t result = sys_getcwd((uint64_t)buf, sizeof(buf), 0, 0, 0, 0);

    if (result > 0) {
        lua_pushstring(L, buf);
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.stat(path)
**
** Get file/directory metadata.
**
** @param path File path
** @return Table with stat info, or nil + error code
*/
static int lvespera_stat(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    vespera_stat_t st;

    int64_t result = sys_stat((uint64_t)path, (uint64_t)&st, 0, 0, 0, 0);

    if (result != 0) {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }

    lua_createtable(L, 0, 10);

    lua_pushinteger(L, (lua_Integer)st.node_type);
    lua_setfield(L, -2, "node_type");

    lua_pushinteger(L, (lua_Integer)st.flags);
    lua_setfield(L, -2, "flags");

    lua_pushinteger(L, (lua_Integer)st.size);
    lua_setfield(L, -2, "size");

    lua_pushinteger(L, (lua_Integer)st.blocks);
    lua_setfield(L, -2, "blocks");

    lua_pushinteger(L, (lua_Integer)st.mtime);
    lua_setfield(L, -2, "mtime");

    lua_pushinteger(L, (lua_Integer)st.inode_id);
    lua_setfield(L, -2, "inode_id");

    return 1;
}

/*
** vespera.readdir(path)
**
** Read directory contents - returns an iterator.
**
** @param path Directory path
** @return Iterator function
*/

typedef struct {
    DIR_HANDLE dir;
    dirent_t entry;
} dir_iterator_t;

#define VESPERA_DIR_META "vespera_dir_iter"

/* __gc: wird vom GC aufgerufen, sobald der Userdata freigegeben wird */
static int dir_iterator_gc(lua_State *L) {
    dir_iterator_t *iter = (dir_iterator_t *)lua_touserdata(L, 1);
    if (iter && iter->dir != (DIR_HANDLE)-1) {
        sys_close((uint64_t)iter->dir, 0, 0, 0, 0, 0);
        iter->dir = (DIR_HANDLE)-1;  /* guard gegen Doppel-close */
    }
    return 0;
}

static int readdir_iter(lua_State *L) {
    /* Userdata als Upvalue – funktioniert mit "local iter = vesp.readdir(...)" */
    dir_iterator_t *iter = (dir_iterator_t *)lua_touserdata(L, lua_upvalueindex(1));

    if (!iter || iter->dir == (DIR_HANDLE)-1) {
        return luaL_error(L, "invalid iterator");
    }

    int64_t result = sys_readdir((uint64_t)iter->dir, (uint64_t)&iter->entry, 0, 0, 0, 0);

    if (result <= 0) {
        sys_close((uint64_t)iter->dir, 0, 0, 0, 0, 0);
        iter->dir = (DIR_HANDLE)-1;
        return 0;
    }

    lua_pushstring(L, iter->entry.name);
    return 1;
}

static int lvespera_readdir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);

    int64_t dir_result = sys_open((uint64_t)path, O_DIRECTORY, 0, 0, 0, 0);
    if (dir_result < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, dir_result);
        return 2;
    }

    dir_iterator_t *iter = (dir_iterator_t *)lua_newuserdata(L, sizeof(dir_iterator_t));
    iter->dir = (DIR_HANDLE)dir_result;

    if (luaL_newmetatable(L, VESPERA_DIR_META)) {
        lua_pushcfunction(L, dir_iterator_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);

    /* Closure: Userdata wird als Upvalue #1 eingebacken, Stack danach leer */
    lua_pushcclosure(L, readdir_iter, 1);  /* <-- 1 statt pushcfunction */
    return 1;                              /* <-- nur die Closure zurückgeben */
}

/* ============================================================================
** System Operations
** ============================================================================ */

/*
** vespera.sleep(seconds)
**
** Sleep for the specified number of seconds.
**
** @param seconds Sleep duration (can be fractional)
** @return true on success, or nil + error code
*/
static int lvespera_sleep(lua_State *L) {
    lua_Number seconds = luaL_checknumber(L, 1);

    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - ts.tv_sec) * 1000000000);

    int64_t result = sys_nanosleep((uint64_t)&ts, 0, 0, 0, 0, 0);

    if (result == 0) {
        return push_success(L);
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.time()
**
** Get current time.
**
** @return Seconds since epoch
*/
static int lvespera_time(lua_State *L) {
    struct timespec ts;

    int64_t result = sys_clock_gettime(0, (uint64_t)&ts, 0, 0, 0, 0);

    if (result == 0) {
        lua_pushnumber(L, (lua_Number)ts.tv_sec + (lua_Number)ts.tv_nsec / 1000000000.0);
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.uptime()
**
** Get monotonic time since system boot.
** Suitable for measuring elapsed time and timeouts.
** Unlike time(), this clock cannot jump backwards.
**
** @return Seconds since boot as a float (millisecond resolution)
*/
static int lvespera_uptime(lua_State *L) {
    struct timespec ts;
    sys_clock_gettime(1, (uint64_t)&ts, 0, 0, 0, 0); /* CLOCK_MONOTONIC */
    lua_pushnumber(L, (lua_Number)ts.tv_sec + (lua_Number)ts.tv_nsec / 1000000000.0);
    return 1;
}

/*
** vespera.reboot()
**
** Reboot the system.
**
** @return Does not return
*/
static int lvespera_reboot(lua_State *L) {
    /* REBOOT_MAGIC1 = 0xfee1dead, REBOOT_MAGIC2 = 0x28121969 */
    sys_reboot(0xfee1dead, 0x28121969, 0, 0, 0, 0);
    /* If we get here, reboot failed */
    lua_pushnil(L);
    lua_pushliteral(L, "reboot failed");
    return 2;
}

/*
** vespera.mount(fstype, path, [device])
**
** Mount a filesystem.
**
** @param fstype Filesystem type ("devfs", "ext4", etc.)
** @param path Mount point
** @param device Optional device path
** @return true on success, or nil + error code
*/
static int lvespera_mount(lua_State *L) {
    const char *fstype = luaL_checkstring(L, 1);
    const char *path = luaL_checkstring(L, 2);
    const char *device = luaL_optstring(L, 3, NULL);

    int64_t result = sys_mount((uint64_t)fstype, (uint64_t)path,
                                device ? (uint64_t)device : 0, 0, 0, 0);

    if (result == 0) {
        return push_success(L);
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.umount(path)
**
** Unmount a filesystem.
**
** @param path Mount point
** @return true on success, or nil + error code
*/
static int lvespera_umount(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);

    int64_t result = sys_umount((uint64_t)path, 0, 0, 0, 0, 0);

    if (result == 0) {
        return push_success(L);
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/* ============================================================================
** Logging
** ============================================================================ */

/*
** vespera.log(message)
**
** Write a message to the kernel log (via stderr handle).
**
** @param message Log message
** @return bytes written on success, or nil + error code
*/
static int lvespera_log(lua_State *L) {
    const char *msg = luaL_checkstring(L, 1);
    size_t len = strlen(msg);

    /* Write to stderr (which goes to kernel log in VesperaOS) */
    int64_t result = sys_write(HANDLE_STDERR, (uint64_t)msg, len, 0, 0, 0);

    if (result >= 0) {
        /* Also write newline */
        sys_write(HANDLE_STDERR, (uint64_t)"\n", 1, 0, 0, 0);
        lua_pushinteger(L, result + 1);
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.logf(format, ...)
**
** Write a formatted message to the kernel log.
**
** @param format Format string
** @param ... Format arguments
** @return true on success
*/
static int lvespera_logf(lua_State *L) {
    const char *fmt = luaL_checkstring(L, 1);
    int nargs = lua_gettop(L);

    /* Build formatted string using Lua's string.format */
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "format");
    lua_pushvalue(L, 1); /* format string */
    for (int i = 2; i <= nargs; i++) {
        lua_pushvalue(L, i);
    }
    lua_call(L, nargs, 1);

    const char *formatted = lua_tostring(L, -1);
    lua_pop(L, 2); /* pop string table and formatted result */

    return lvespera_log(L);
}

/* ============================================================================
** Channel/IPC
** ============================================================================ */

/*
** vespera.channel_create([capacity])
**
** Create an IPC channel.
**
** @param capacity Optional buffer capacity (default: 4096)
** @return Channel handle on success, or nil + error code
*/
static int lvespera_channel_create(lua_State *L) {
    uint64_t capacity = (uint64_t)luaL_optinteger(L, 1, 4096);

    int64_t result = sys_channel_create(capacity, 0, 0, 0, 0, 0);

    if (result > 0) {
        lua_pushinteger(L, (lua_Integer)result);
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.channel_send(handle, data)
**
** Send data on a channel.
**
** @param handle Channel handle
** @param data Data to send
** @return Bytes sent on success, or nil + error code
*/
static int lvespera_channel_send(lua_State *L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);

    int64_t result = sys_channel_send(handle, (uint64_t)data, len, 0, 0, 0);

    if (result >= 0) {
        lua_pushinteger(L, (lua_Integer)result);
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/*
** vespera.channel_recv(handle, size)
**
** Receive data from a channel.
**
** @param handle Channel handle
** @param size Maximum bytes to receive
** @return Data string on success, or nil + error code
*/
static int lvespera_channel_recv(lua_State *L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    size_t size = (size_t)luaL_checkinteger(L, 2);

    char *buf = (char *)malloc(size);
    if (!buf) {
        lua_pushnil(L);
        lua_pushliteral(L, "out of memory");
        return 2;
    }

    int64_t result = sys_channel_recv(handle, (uint64_t)buf, size, 0, 0, 0);

    if (result > 0) {
        lua_pushlstring(L, buf, (size_t)result);
        free(buf);
        return 1;
    } else {
        free(buf);
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/* ============================================================================
** Pipe IPC
** ============================================================================ */

/*
** vespera.pipe()
**
** Create a pipe.
**
** @return read_handle, write_handle on success, or nil + error code
*/
static int lvespera_pipe(lua_State *L) {
    uint64_t fds[2];

    int64_t result = sys_pipe((uint64_t)fds, 0, 0, 0, 0, 0);

    if (result == 0) {
        lua_pushinteger(L, (lua_Integer)fds[0]); /* read end */
        lua_pushinteger(L, (lua_Integer)fds[1]); /* write end */
        return 2;
    } else {
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }
}

/* ============================================================================
** Poll
** ============================================================================ */

/*
** vespera.poll(handles, timeout_ms)
**
** Wait for events on handles.
**
** @param handles Table of handle IDs to poll
** @param timeout_ms Timeout in milliseconds (-1 = infinite)
** @return Table of ready handles, or nil + error code
*/
static int lvespera_poll(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int64_t timeout = luaL_optinteger(L, 2, -1);

    int nhandles = (int)luaL_len(L, 1);
    if (nhandles == 0) {
        lua_pushnil(L);
        lua_pushliteral(L, "no handles to poll");
        return 2;
    }

    struct pollhdl *pfds = (struct pollhdl *)malloc(nhandles * sizeof(struct pollhdl));
    if (!pfds) {
        lua_pushnil(L);
        lua_pushliteral(L, "out of memory");
        return 2;
    }

    for (int i = 0; i < nhandles; i++) {
        lua_rawgeti(L, 1, i + 1);
        pfds[i].hdl = (i64)lua_tointeger(L, -1);
        pfds[i].events = POLLIN;
        pfds[i].revents = 0;
        lua_pop(L, 1);
    }

    int64_t result = sys_poll((uint64_t)pfds, (uint64_t)nhandles, (uint64_t)timeout, 0, 0, 0);

    if (result < 0) {
        free(pfds);
        lua_pushnil(L);
        lua_pushinteger(L, result);
        return 2;
    }

    /* Build result table with ready handles */
    lua_createtable(L, (int)result, 0);
    int n = 0;
    for (int i = 0; i < nhandles; i++) {
        if (pfds[i].revents & (POLLIN | POLLERR | POLLHUP)) {
            lua_pushinteger(L, (lua_Integer)pfds[i].hdl);
            lua_rawseti(L, -2, ++n);
        }
    }

    free(pfds);
    return 1;
}

/* ============================================================================
** Module Registration
** ============================================================================ */

static const luaL_Reg vesp_lib[] = {
    /* Process */
    {"spawn", lvespera_spawn},
    {"exit", lvespera_exit},
    {"wait", lvespera_wait},
    {"kill", lvespera_kill},
    {"getrid", lvespera_getrid},

    /* File I/O */
    {"open", lvespera_open},
    {"close", lvespera_close},
    {"read", lvespera_read},
    {"write", lvespera_write},
    {"seek", lvespera_seek},

    /* Filesystem */
    {"mkdir", lvespera_mkdir},
    {"unlink", lvespera_unlink},
    {"rmdir", lvespera_rmdir},
    {"rename", lvespera_rename},
    {"chdir", lvespera_chdir},
    {"getcwd", lvespera_getcwd},
    {"stat", lvespera_stat},
    {"readdir", lvespera_readdir},

    /* System */
    {"sleep", lvespera_sleep},
    {"time", lvespera_time},
    {"uptime", lvespera_uptime},
    {"reboot", lvespera_reboot},
    {"mount", lvespera_mount},
    {"umount", lvespera_umount},

    /* Logging */
    {"log", lvespera_log},
    {"logf", lvespera_logf},

    /* IPC */
    {"channel_create", lvespera_channel_create},
    {"channel_send", lvespera_channel_send},
    {"channel_recv", lvespera_channel_recv},
    {"pipe", lvespera_pipe},
    {"poll", lvespera_poll},

    {NULL, NULL}
};

/* Constants */
static const struct {
    const char *name;
    lua_Integer value;
} vesp_constants[] = {
    /* Seek modes */
    {"SEEK_SET", SEEK_SET},
    {"SEEK_CUR", SEEK_CUR},
    {"SEEK_END", SEEK_END},

    /* Handle types */
    {"HANDLE_STDIN", (lua_Integer)HANDLE_STDIN},
    {"HANDLE_STDOUT", (lua_Integer)HANDLE_STDOUT},
    {"HANDLE_STDERR", (lua_Integer)HANDLE_STDERR},

    /* Signals */
    {"SIGKILL", SIGKILL},
    {"SIGTERM", SIGTERM},
    {"SIGINT", SIGINT},

    /* Poll flags */
    {"POLLIN", POLLIN},
    {"POLLOUT", POLLOUT},
    {"POLLERR", POLLERR},
    {"POLLHUP", POLLHUP},

    /* Open flags */
    {"O_RDONLY", O_RDONLY},
    {"O_WRONLY", O_WRONLY},
    {"O_RDWR", O_RDWR},

    {NULL, 0}
};

LUAMOD_API int luaopen_vespera(lua_State *L) {
    luaL_newlib(L, vesp_lib);

    /* Register constants */
    for (int i = 0; vesp_constants[i].name != NULL; i++) {
        lua_pushinteger(L, vesp_constants[i].value);
        lua_setfield(L, -2, vesp_constants[i].name);
    }

    return 1;
}
