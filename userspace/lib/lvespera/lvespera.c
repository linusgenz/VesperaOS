// lvespera.c
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 20.04.26.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

// VesperaOS system module for Lua
//
// Provides Lua bindings for VesperaOS syscalls

#define lvespera_c
#define LUA_LIB

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

/* VesperaOS headers */
#include <dirent.h>
#include <errno.h>
#include <fflags.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysstd.h>
#include <time.h>
#include <vbus.h>
#include <vespera/capabilities.h>
#include <vespera/handles.h>
#include <vespera/mount.h>
#include <vespera/poll.h>
#include <vespera/spawn.h>
#include <vespera/stat.h>

/* ============================================================================
** Internal helpers
** ============================================================================ */

/* Push nil + negative error code and return 2. */
static int push_errno(lua_State* L, int64_t err) {
    lua_pushnil(L);
    lua_pushinteger(L, (lua_Integer)err);
    return 2;
}

/* Push true and return 1. */
static int push_ok(lua_State* L) {
    lua_pushboolean(L, 1);
    return 1;
}

/*
** push_result: If result >= 0 push it and return 1.
**              If result <  0 push nil + result and return 2.
*/
static int push_result(lua_State* L, int64_t result) {
    if (result < 0) return push_errno(L, result);
    lua_pushinteger(L, (lua_Integer)result);
    return 1;
}

/* ============================================================================
** vespera.proc  — Process / realm / unit management
** ============================================================================ */

/*
** proc.spawn(path [, args_table [, env_table]])
**
** Spawns a new realm from the given executable.
** args_table  : array of strings  {"arg0", "arg1", …}  (optional)
** env_table   : array of "K=V" strings                 (optional)
**
** config_table (optional):
**   {
**     stdin  = i64,   -- replace STDIN handle in child (0 = inherit TTY)
**     stdout = i64,   -- replace STDOUT handle in child (0 = inherit TTY)
**     stderr = i64,   -- replace STDERR handle in child (0 = inherit TTY)
**
**     bg     = bool,  -- if true, realm detaches from controlling TTY
**
**     name   = string -- optional realm name (debug / tooling)
**   }
**
** Returns realm_id on success, or nil + error_code.
*/
static int lproc_spawn(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);

    /* Build argv */
    const char* argv_static[64];
    const char** argv = argv_static;
    int argc = 0;
    argv[argc++] = path; /* argv[0] is always the path */

    if (lua_type(L, 2) == LUA_TTABLE) {
        int n = (int)luaL_len(L, 2);
        for (int i = 1; i <= n && argc < 63; i++) {
            lua_rawgeti(L, 2, i);
            argv[argc++] = lua_tostring(L, -1);
            lua_pop(L, 1);
        }
    }
    argv[argc] = NULL;

    /* Build envp */
    const char* envp_static[64];
    const char** envp = envp_static;
    int envc = 0;

    if (lua_type(L, 3) == LUA_TTABLE) {
        if (luaL_len(L, 3) > 0) {
            int n = (int)luaL_len(L, 3);
            for (int i = 1; i <= n && envc < 63; i++) {
                lua_rawgeti(L, 3, i);
                envp[envc++] = lua_tostring(L, -1);
                lua_pop(L, 1);
            }
        } else {
            lua_pushnil(L);
            while (lua_next(L, 3) != 0 && envc < 63) {
                const char* key = lua_tostring(L, -2);
                const char* val = lua_tostring(L, -1);

                if (key && val) {
                    static char buf[64][256];
                    snprintf(buf[envc], sizeof(buf[envc]), "%s=%s", key, val);
                    envp[envc++] = buf[envc];
                }

                lua_pop(L, 1);
            }
        }
    }
    envp[envc] = NULL;

    spawn_config_t cfg = {};

    if (lua_type(L, 4) == LUA_TTABLE) {
        cfg.stdin_handle = 0;
        cfg.stdout_handle = 0;
        cfg.stderr_handle = 0;
        cfg.bg_realm = 0;
        cfg.realm_name = NULL;
        cfg.uid = 0;
        cfg.gid = 0;
        cfg.home = NULL;

        lua_getfield(L, 4, "stdin");
        if (lua_isnumber(L, -1)) cfg.stdin_handle = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 4, "stdout");
        if (lua_isnumber(L, -1)) cfg.stdout_handle = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 4, "stderr");
        if (lua_isnumber(L, -1)) cfg.stderr_handle = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 4, "bg");
        if (lua_isboolean(L, -1)) cfg.bg_realm = lua_toboolean(L, -1) ? 1 : 0;
        lua_pop(L, 1);

        lua_getfield(L, 4, "name");
        if (lua_isstring(L, -1)) cfg.realm_name = (char*)lua_tostring(L, -1);

        lua_getfield(L, 4, "home");
        if (lua_isstring(L, -1)) cfg.home = (char*)lua_tostring(L, -1);

        lua_getfield(L, 4, "uid");
        if (lua_isnumber(L, -1)) cfg.uid = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 4, "gid");
        if (lua_isnumber(L, -1)) cfg.gid = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_pop(L, 1);
    }

    int64_t result = sys_spawn((uint64_t)path, (uint64_t)argv, (uint64_t)envp, (uint64_t)&cfg, 0, 0);
    return push_result(L, result);
}

/*
** proc.spawn_unit(realm_id, entry_addr, arg_ptr [, stack_size])
**
** Spawns a new unit (thread) inside an existing realm.
** entry_addr  : user-space function address (integer)
** arg_ptr     : opaque argument forwarded in RDI (integer, may be 0)
** stack_size  : optional, 0 = kernel default
**
** Returns unit_id on success, or nil + error_code.
*/
static int lproc_spawn_unit(lua_State* L) {
    uint64_t realm_id = (uint64_t)luaL_checkinteger(L, 1);
    uint64_t entry = (uint64_t)luaL_checkinteger(L, 2);
    uint64_t arg = (uint64_t)luaL_optinteger(L, 3, 0);
    uint64_t stack_sz = (uint64_t)luaL_optinteger(L, 4, 0);

    int64_t result = sys_unit_spawn(realm_id, entry, arg, stack_sz, 0, 0);
    return push_result(L, result);
}

/*
** proc.exit([code])
**
** Terminate the current unit. Does not return.
*/
static int lproc_exit(lua_State* L) {
    uint64_t code = (uint64_t)luaL_optinteger(L, 1, 0);
    sys_exit(code, 0, 0, 0, 0, 0);
    return 0; /* unreachable */
}

/*
** proc.wait(realm_id)
**
** Block until realm_id terminates.
** Returns exit_code on success, or nil + error_code.
*/
static int lproc_wait(lua_State* L) {
    uint64_t realm = (uint64_t)luaL_checkinteger(L, 1);
    int nohang = lua_toboolean(L, 2); /* defaults to false/0 if absent */
    uint32_t flags = nohang ? WAIT_FLAG_NOHANG : WAIT_FLAG_NONE;

    int exit_code = 0;
    int64_t result = sys_wait(realm, (uint64_t)&exit_code, (uint64_t)flags, 0, 0, 0);

    if (result < 0) {
        return push_errno(L, result);
    }

    if (result == 0) {
        lua_pushnil(L);
        lua_pushliteral(L, "pending");
        return 2;
    }

    lua_pushinteger(L, (lua_Integer)exit_code);
    return 1;
}

/*
** proc.kill(realm_id [, signum])
**
** Send a signal to a realm. signum defaults to SIGKILL (9).
** Returns true on success, or nil + error_code.
*/
static int lproc_kill(lua_State* L) {
    uint64_t realm = (uint64_t)luaL_checkinteger(L, 1);
    int sig = (int)luaL_optinteger(L, 2, SIGKILL);
    int64_t result = sys_kill(realm, (uint64_t)sig, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    return push_ok(L);
}

/*
** proc.getrid()
**
** Returns realm_id, unit_id of the calling process.
*/
static int lproc_getrid(lua_State* L) {
    uint64_t realm_id = 0, unit_id = 0;
    sys_getrid((uint64_t)&realm_id, (uint64_t)&unit_id, 0, 0, 0, 0);
    lua_pushinteger(L, (lua_Integer)realm_id);
    lua_pushinteger(L, (lua_Integer)unit_id);
    return 2;
}

/*
** proc.getuid()
**
** Returns the numeric UID of the calling process.
*/
static int lproc_getuid(lua_State* L) {
    int64_t result = sys_getuid(0, 0, 0, 0, 0, 0);
    return push_result(L, result);
}

/*
** proc.sigaction(signum, handler_fn [, flags])
**
** Register a Lua function as a signal handler.
** handler_fn  : Lua function — will be called with (signum) when triggered.
**               Pass nil to reset to SIG_DFL.
** flags       : SA_* bitmask (optional, default 0)
**
** NOTE: Because Lua signal handlers run asynchronously, only simple, safe
**       operations should be performed inside handler_fn. Prefer using a
**       channel or flag variable and polling from the main loop instead.
**
** Returns true on success, or nil + error_code.
*/

/*
** We store handler refs in a global registry table keyed by signal number.
** The C-level trampoline calls back into Lua when a signal fires.
** Since VesperaOS delivers signals synchronously (on next syscall return),
** this is safe.
*/
static lua_State* g_signal_L = NULL;
static int g_signal_handlers[NSIG]; /* LUA_NOREF or registry ref */

static void signal_trampoline(int signum) {
    if (!g_signal_L) return;
    if (signum < 0 || signum >= NSIG) return;
    int ref = g_signal_handlers[signum];
    if (ref == LUA_NOREF) return;

    lua_rawgeti(g_signal_L, LUA_REGISTRYINDEX, ref);
    lua_pushinteger(g_signal_L, (lua_Integer)signum);
    lua_pcall(g_signal_L, 1, 0, 0);
}

static int lproc_sigaction(lua_State* L) {
    int signum = (int)luaL_checkinteger(L, 1);
    if (signum < 0 || signum >= NSIG) return luaL_error(L, "invalid signal number %d", signum);

    if (!g_signal_L) {
        g_signal_L = L;
        for (int i = 0; i < NSIG; i++) g_signal_handlers[i] = LUA_NOREF;
    }

    /* Release previous handler reference */
    if (g_signal_handlers[signum] != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, g_signal_handlers[signum]);
        g_signal_handlers[signum] = LUA_NOREF;
    }

    struct sigaction sa;
    if (lua_type(L, 2) == LUA_TNIL || lua_type(L, 2) == LUA_TNONE) {
        sa.sa_handler = SIG_DFL;
    } else {
        luaL_checktype(L, 2, LUA_TFUNCTION);
        lua_pushvalue(L, 2);
        g_signal_handlers[signum] = luaL_ref(L, LUA_REGISTRYINDEX);
        sa.sa_handler = signal_trampoline;
    }

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = (int)luaL_optinteger(L, 3, 0);

    int64_t result = sys_sigaction((uint64_t)signum, (uint64_t)&sa, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    return push_ok(L);
}

/*
** proc.handle_transfer(handle, target_realm_id [, caps_mask])
**
** Transfer a handle (channel or pipe) to another realm.
** handle          : handle to transfer (must be transferable)
** target_realm_id : destination realm
** caps_mask       : capability bitmask to grant (default CAP_ALL)
**                   The actually granted caps are (src_caps & caps_mask).
**
** Returns the new handle_id in the target realm on success,
** or nil + error_code on failure.
*/
static int lproc_handle_transfer(lua_State* L) {
    uint64_t hid = (uint64_t)luaL_checkinteger(L, 1);
    uint64_t target_realm_id = (uint64_t)luaL_checkinteger(L, 2);
    uint64_t caps_mask = (uint64_t)luaL_optinteger(L, 3, (lua_Integer)CAP_ALL);

    int64_t result = sys_handle_transfer(hid, target_realm_id, caps_mask, 0, 0, 0);
    return push_result(L, result);
}

static const luaL_Reg proc_lib[] = {
    {"spawn", lproc_spawn},
    {"spawn_unit", lproc_spawn_unit},
    {"exit", lproc_exit},
    {"wait", lproc_wait},
    {"kill", lproc_kill},
    {"getrid", lproc_getrid},
    {"getuid", lproc_getuid},
    {"sigaction", lproc_sigaction},
    {"handle_transfer", lproc_handle_transfer},
    {NULL, NULL}
};

/* ============================================================================
** vespera.io  — Low-level file I/O
** ============================================================================ */

/*
** io.open(path [, flags])
**
** Open a file or device.
** flags : integer bitmask (O_RDONLY / O_WRONLY / O_RDWR / O_CREAT / O_TRUNC).
**         Defaults to O_RDONLY.
** Also accepts POSIX-style mode string ("r", "w", "r+", "w+", "a").
**
** Returns handle on success, or nil + error_code.
*/
static int lio_open(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int flags;

    if (lua_type(L, 2) == LUA_TSTRING) {
        const char* mode = lua_tostring(L, 2);
        if (strcmp(mode, "r") == 0)
            flags = O_RDONLY;
        else if (strcmp(mode, "w") == 0)
            flags = O_WRONLY | O_CREAT | O_TRUNC;
        else if (strcmp(mode, "a") == 0)
            flags = O_WRONLY | O_CREAT | O_APPEND;
        else if (strcmp(mode, "r+") == 0)
            flags = O_RDWR;
        else if (strcmp(mode, "w+") == 0)
            flags = O_RDWR | O_CREAT | O_TRUNC;
        else if (strcmp(mode, "a+") == 0)
            flags = O_RDWR | O_CREAT | O_APPEND;
        else
            return luaL_error(L, "unknown file mode '%s'", mode);
    } else {
        flags = (int)luaL_optinteger(L, 2, O_RDONLY);
    }

    int64_t handle = sys_open((uint64_t)path, (uint64_t)flags, 0, 0, 0, 0);
    return push_result(L, handle);
}

/*
** io.close(handle)
**
** Close a file or device handle.
** Returns true on success, or nil + error_code.
*/
static int lio_close(lua_State* L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    int64_t result = sys_close(handle, 0, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    return push_ok(L);
}

/*
** io.read(handle, count)
**
** Read up to count bytes from handle.
** Returns data string on success (may be shorter than count at EOF),
** or nil + error_code on failure.
** Returns empty string "" at EOF.
*/
static int lio_read(lua_State* L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    size_t count = (size_t)luaL_checkinteger(L, 2);

    char* buf = (char*)malloc(count);
    if (!buf) return luaL_error(L, "out of memory");

    int64_t result = sys_read(handle, (uint64_t)buf, count, 0, 0, 0);
    if (result < 0) {
        free(buf);
        return push_errno(L, result);
    }
    lua_pushlstring(L, buf, (size_t)result);
    free(buf);
    return 1;
}

/*
** io.read_all(handle)
**
** Read the entire file into a string, starting from the current position.
** Uses repeated 4096-byte reads and concatenates via luaL_Buffer.
**
** Returns data string on success, or nil + error_code.
*/
static int lio_read_all(lua_State* L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    luaL_Buffer B;
    luaL_buffinit(L, &B);

    char tmp[4096];
    for (;;) {
        int64_t n = sys_read(handle, (uint64_t)tmp, sizeof(tmp), 0, 0, 0);
        if (n < 0) {
            /* Return what we have so far, or propagate the error */
            luaL_pushresult(&B);
            if (lua_rawlen(L, -1) == 0) {
                lua_pop(L, 1);
                return push_errno(L, n);
            }
            return 1;
        }
        if (n == 0) break; /* EOF */
        luaL_addlstring(&B, tmp, (size_t)n);
    }
    luaL_pushresult(&B);
    return 1;
}

/*
** io.write(handle, data)
**
** Write data string to handle.
** Returns bytes_written on success, or nil + error_code.
*/
static int lio_write(lua_State* L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    size_t len;
    const char* data = luaL_checklstring(L, 2, &len);
    int64_t result = sys_write(handle, (uint64_t)data, len, 0, 0, 0);
    return push_result(L, result);
}

/*
** io.seek(handle, offset [, whence])
**
** Reposition the file offset.
** whence defaults to SEEK_SET.
** Returns new absolute offset on success, or nil + error_code.
*/
static int lio_seek(lua_State* L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    int64_t offset = (int64_t)luaL_checkinteger(L, 2);
    int64_t whence = (int64_t)luaL_optinteger(L, 3, SEEK_SET);
    int64_t result = sys_seek(handle, (uint64_t)offset, (uint64_t)whence, 0, 0, 0);
    return push_result(L, result);
}

/*
** io.ioctl(handle, request, [arg_table])
**
** Perform a device-specific ioctl.
** arg_table : optional table whose fields are copied into a uint64_t arg.
**             For simple requests, pass the arg as a plain integer in field [1].
**
** Returns the ioctl result (device-dependent) on success, or nil + error_code.
*/
static int lio_ioctl(lua_State* L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    uint64_t request = (uint64_t)luaL_checkinteger(L, 2);
    uint64_t arg = 0;

    if (lua_type(L, 3) == LUA_TNUMBER) {
        arg = (uint64_t)lua_tointeger(L, 3);
    } else if (lua_type(L, 3) == LUA_TUSERDATA || lua_type(L, 3) == LUA_TLIGHTUSERDATA) {
        arg = (uint64_t)(uintptr_t)lua_touserdata(L, 3);
    }
    /* Pointers to Lua userdata can be passed for more complex ioctls */

    int64_t result = sys_ioctl(handle, request, arg, 0, 0, 0);
    return push_result(L, result);
}

static const luaL_Reg io_lib[] = {
    {"open", lio_open},
    {"close", lio_close},
    {"read", lio_read},
    {"read_all", lio_read_all},
    {"write", lio_write},
    {"seek", lio_seek},
    {"ioctl", lio_ioctl},
    {NULL, NULL}
};

/* ============================================================================
** vespera.fs  — Filesystem operations
** ============================================================================ */

/*
** fs.mkdir(path)  — create a directory
** fs.rmdir(path)  — remove an empty directory
** fs.unlink(path) — remove a file
** fs.rename(old, new) — rename / move
** fs.chdir(path)  — change working directory
** fs.getcwd()     — get current working directory
** fs.stat(path)   — query file metadata
** fs.exists(path) — return true/false
** fs.readdir(path) — return iterator over entry names
** fs.mount(source, target, fstype [, flags])
** fs.umount(target [, flags])
** fs.create(path [, flags]) — create file or directory
*/

static int lfs_mkdir(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int64_t result = sys_mkdir((uint64_t)path, 0, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    return push_ok(L);
}

static int lfs_rmdir(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int64_t result = sys_rmdir((uint64_t)path, 0, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    return push_ok(L);
}

static int lfs_unlink(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int64_t result = sys_unlink((uint64_t)path, 0, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    return push_ok(L);
}

static int lfs_rename(lua_State* L) {
    const char* oldpath = luaL_checkstring(L, 1);
    const char* newpath = luaL_checkstring(L, 2);
    int64_t result = sys_rename((uint64_t)oldpath, (uint64_t)newpath, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    return push_ok(L);
}

static int lfs_chdir(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int64_t result = sys_chdir((uint64_t)path, 0, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    return push_ok(L);
}

static int lfs_getcwd(lua_State* L) {
    char buf[1024];
    int64_t result = sys_getcwd((uint64_t)buf, sizeof(buf), 0, 0, 0, 0);
    if (result <= 0) return push_errno(L, result);
    lua_pushstring(L, buf);
    return 1;
}

/*
** fs.stat(path)
**
** Returns a table with the following fields:
**   node_type  : integer (VSTAT_TYPE_*: 0=unknown, 1=file, 2=dir, 3=chardev, 4=blockdev, 5=symlink)
**   flags      : integer (VSTAT_FLAG_* bitmask: readable, writable, exec, virtual, permanent)
**   size       : integer (file size in bytes)
**   blocks     : integer (allocated 512-byte blocks)
**   inode_id   : integer
**   mtime      : integer (Unix seconds)
**   atime      : integer (Unix seconds)
**   ctime      : integer (Unix seconds)
**   uid        : integer
**   gid        : integer
**   mode       : integer (raw Unix permission bits, e.g. 0644)
**   is_file    : boolean
**   is_dir     : boolean
*/
static int lfs_stat(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    vespera_stat_t st;
    int64_t result = sys_stat((uint64_t)path, (uint64_t)&st, 0, 0, 0, 0);
    if (result != 0) return push_errno(L, result);

    lua_createtable(L, 0, 14);

#define SET_INT(field)                         \
    lua_pushinteger(L, (lua_Integer)st.field); \
    lua_setfield(L, -2, #field)
    SET_INT(node_type);
    SET_INT(flags);
    SET_INT(size);
    SET_INT(blocks);
    SET_INT(inode_id);
    SET_INT(mtime);
    SET_INT(atime);
    SET_INT(ctime);
    SET_INT(uid);
    SET_INT(gid);
    SET_INT(mode);
#undef SET_INT

    lua_pushboolean(L, st.node_type == VSTAT_TYPE_FILE);
    lua_setfield(L, -2, "is_file");
    lua_pushboolean(L, st.node_type == VSTAT_TYPE_DIR);
    lua_setfield(L, -2, "is_dir");
    return 1;
}

/*
** fs.exists(path) — returns true/false (never errors)
*/
static int lfs_exists(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    vespera_stat_t st;
    int64_t result = sys_stat((uint64_t)path, (uint64_t)&st, 0, 0, 0, 0);
    lua_pushboolean(L, result == 0);
    return 1;
}

/*
** fs.readdir(path) — returns an iterator: for name in vespera.fs.readdir("/") do … end
** Each call of the iterator returns one entry name string, or nil when done.
*/

typedef struct {
    DIR_HANDLE dir;
    dirent_t entry;
} dir_iter_t;

#define VESPERA_DIR_META "vespera.dir_iter"

static int dir_iter_gc(lua_State* L) {
    dir_iter_t* iter = (dir_iter_t*)lua_touserdata(L, 1);
    if (iter && iter->dir != (DIR_HANDLE)-1) {
        sys_close((uint64_t)iter->dir, 0, 0, 0, 0, 0);
        iter->dir = (DIR_HANDLE)-1;
    }
    return 0;
}

static int dir_iter_next(lua_State* L) {
    dir_iter_t* iter = (dir_iter_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (!iter || iter->dir == (DIR_HANDLE)-1) return 0;

    int64_t result = sys_readdir((uint64_t)iter->dir, (uint64_t)&iter->entry, 0, 0, 0, 0);
    if (result <= 0) {
        sys_close((uint64_t)iter->dir, 0, 0, 0, 0, 0);
        iter->dir = (DIR_HANDLE)-1;
        return 0;
    }
    lua_pushstring(L, iter->entry.name);
    return 1;
}

static int lfs_readdir(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int64_t dir_h = sys_open((uint64_t)path, O_DIRECTORY, 0, 0, 0, 0);
    if (dir_h < 0) return push_errno(L, dir_h);

    dir_iter_t* iter = (dir_iter_t*)lua_newuserdata(L, sizeof(dir_iter_t));
    iter->dir = (DIR_HANDLE)dir_h;

    if (luaL_newmetatable(L, VESPERA_DIR_META)) {
        lua_pushcfunction(L, dir_iter_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    lua_pushcclosure(L, dir_iter_next, 1);
    return 1;
}

/*
** fs.readdir_table(path) — convenience: returns an array table of all entry names
*/
static int lfs_readdir_table(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int64_t dir_h = sys_open((uint64_t)path, O_DIRECTORY, 0, 0, 0, 0);
    if (dir_h < 0) return push_errno(L, dir_h);

    lua_newtable(L);
    int idx = 1;
    dirent_t entry;
    while (sys_readdir((uint64_t)dir_h, (uint64_t)&entry, 0, 0, 0, 0) > 0) {
        lua_pushstring(L, entry.name);
        lua_rawseti(L, -2, idx++);
    }
    sys_close((uint64_t)dir_h, 0, 0, 0, 0, 0);
    return 1;
}

/*
** fs.mount(source, target, fstype [, flags])
** fs.umount(target [, flags])
*/
static int lfs_mount(lua_State* L) {
    const char* source = luaL_checkstring(L, 1);
    const char* target = luaL_checkstring(L, 2);
    const char* fstype = luaL_checkstring(L, 3);
    int64_t flags = luaL_optinteger(L, 4, 0);
    int64_t result = sys_mount((uint64_t)source, (uint64_t)target, (uint64_t)fstype, (uint64_t)flags, 0, 0);
    if (result < 0) return push_errno(L, result);
    return push_ok(L);
}

static int lfs_umount(lua_State* L) {
    const char* target = luaL_checkstring(L, 1);
    int64_t flags = luaL_optinteger(L, 2, 0);
    int64_t result = sys_umount((uint64_t)target, (uint64_t)flags, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    return push_ok(L);
}

/*
** fs.create(path [, flags])
** Creates a file (C_FILE) by default, or a directory if flags == C_DIR.
*/
static int lfs_create(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int flags = (int)luaL_optinteger(L, 2, C_FILE);
    int64_t result;
    if (flags == C_DIR)
        result = sys_mkdir((uint64_t)path, 0, 0, 0, 0, 0);
    else
        result = sys_create((uint64_t)path, 0, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    return push_ok(L);
}

static const luaL_Reg fs_lib[] = {
    {"mkdir", lfs_mkdir},
    {"rmdir", lfs_rmdir},
    {"unlink", lfs_unlink},
    {"rename", lfs_rename},
    {"chdir", lfs_chdir},
    {"getcwd", lfs_getcwd},
    {"stat", lfs_stat},
    {"exists", lfs_exists},
    {"readdir", lfs_readdir},
    {"readdir_table", lfs_readdir_table},
    {"mount", lfs_mount},
    {"umount", lfs_umount},
    {"create", lfs_create},
    {NULL, NULL}
};

/* ============================================================================
** vespera.ipc  — Inter-process communication: channels, pipes, poll
** ============================================================================ */

/*
** ipc.channel_create([capacity])
**
** Create a new IPC channel. capacity defaults to 4096 bytes.
** Returns channel_handle on success, or nil + error_code.
*/
static int lipc_channel_create(lua_State* L) {
    uint64_t cap = (uint64_t)luaL_optinteger(L, 1, 4096);
    int64_t result = sys_channel_create(cap, 0, 0, 0, 0, 0);
    return push_result(L, result);
}

/*
** ipc.channel_send(handle, data)
**
** Write data (string) to a channel.
** Returns bytes_sent on success, or nil + error_code.
** -EAGAIN means the channel is full (non-blocking).
*/
static int lipc_channel_send(lua_State* L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    size_t len;
    const char* data = luaL_checklstring(L, 2, &len);
    int64_t result = sys_channel_send(handle, (uint64_t)data, (uint64_t)len, 0, 0, 0);
    return push_result(L, result);
}

/*
** ipc.channel_recv(handle, max_bytes)
**
** Read up to max_bytes from a channel.
** Returns data_string on success, or nil + error_code.
** -EAGAIN means the channel is empty.
*/
static int lipc_channel_recv(lua_State* L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);
    size_t size = (size_t)luaL_checkinteger(L, 2);
    char* buf = (char*)malloc(size);
    if (!buf) return luaL_error(L, "out of memory");
    int64_t result = sys_channel_recv(handle, (uint64_t)buf, (uint64_t)size, 0, 0, 0);
    if (result < 0) {
        free(buf);
        return push_errno(L, result);
    }
    lua_pushlstring(L, buf, (size_t)result);
    free(buf);
    return 1;
}

/*
** ipc.pipe()
**
** Create a unidirectional pipe.
** Returns read_handle, write_handle on success, or nil + error_code.
*/
static int lipc_pipe(lua_State* L) {
    uint64_t fds[2];
    int64_t result = sys_pipe((uint64_t)fds, 0, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    lua_pushinteger(L, (lua_Integer)fds[0]);
    lua_pushinteger(L, (lua_Integer)fds[1]);
    return 2;
}

/*
** ipc.poll(handles_or_pollspec, timeout_ms)
**
** Wait for I/O events on one or more handles.
**
** Two input formats are supported:
**
** 1. Simple array:  { handle1, handle2, … }
**    All handles are monitored for POLLIN | POLLERR | POLLHUP.
**
** 2. Array of spec tables:  { {hdl=h, events=POLLIN|POLLOUT}, … }
**    Allows per-handle event masks.
**
** timeout_ms:  -1 = block forever, 0 = non-blocking, >0 = milliseconds.
**
** Returns a table of result tables on success:
**   { { hdl = handle, revents = bitmask }, … }
** (only entries with non-zero revents are included)
**
** Returns empty table if the timeout expired.
** Returns nil + error_code on failure.
*/
static int lipc_poll(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int64_t timeout = luaL_optinteger(L, 2, -1);

    int n = (int)luaL_len(L, 1);
    if (n == 0) {
        lua_createtable(L, 0, 0);
        return 1;
    }

    struct pollhdl* pfds = (struct pollhdl*)malloc((size_t)n * sizeof(struct pollhdl));
    if (!pfds) return luaL_error(L, "out of memory");

    for (int i = 0; i < n; i++) {
        lua_rawgeti(L, 1, i + 1);
        if (lua_type(L, -1) == LUA_TTABLE) {
            /* spec table: { hdl = h, events = mask } */
            lua_getfield(L, -1, "hdl");
            pfds[i].hdl = (int64_t)lua_tointeger(L, -1);
            lua_pop(L, 1);
            lua_getfield(L, -1, "events");
            pfds[i].events = lua_isnil(L, -1) ? (POLLIN | POLLERR | POLLHUP) : (uint32_t)lua_tointeger(L, -1);
            lua_pop(L, 1);
        } else {
            /* plain handle integer */
            pfds[i].hdl = (int64_t)lua_tointeger(L, -1);
            pfds[i].events = POLLIN | POLLERR | POLLHUP;
        }
        pfds[i].revents = 0;
        lua_pop(L, 1);
    }

    int64_t result = sys_poll((uint64_t)pfds, (uint64_t)n, (uint64_t)timeout, 0, 0, 0);
    if (result < 0) {
        free(pfds);
        return push_errno(L, result);
    }

    lua_createtable(L, (int)result, 0);
    int out = 0;
    for (int i = 0; i < n; i++) {
        if (pfds[i].revents == 0) continue;
        lua_createtable(L, 0, 2);
        lua_pushinteger(L, (lua_Integer)pfds[i].hdl);
        lua_setfield(L, -2, "hdl");
        lua_pushinteger(L, (lua_Integer)pfds[i].revents);
        lua_setfield(L, -2, "revents");
        lua_rawseti(L, -2, ++out);
    }

    free(pfds);
    return 1;
}

static const luaL_Reg ipc_lib[] = {
    {"channel_create", lipc_channel_create},
    {"channel_send", lipc_channel_send},
    {"channel_recv", lipc_channel_recv},
    {"pipe", lipc_pipe},
    {"poll", lipc_poll},
    {NULL, NULL}
};

/* ============================================================================
** vespera.sys  — System: time, sleep, reboot, env
** ============================================================================ */

/*
** sys.sleep(seconds)
**
** Sleep for the specified duration. Accepts fractions (e.g. 0.25).
** Returns true on success, or nil + error_code if interrupted.
*/
static int lsys_sleep(lua_State* L) {
    lua_Number secs = luaL_checknumber(L, 1);
    struct timespec ts;
    ts.tv_sec = (time_t)secs;
    ts.tv_nsec = (long)((secs - (lua_Number)ts.tv_sec) * 1000000000.0);
    int64_t result = sys_nanosleep((uint64_t)&ts, 0, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    return push_ok(L);
}

/*
** sys.sleep_ms(milliseconds) — convenience wrapper
*/
static int lsys_sleep_ms(lua_State* L) {
    lua_Integer ms = luaL_checkinteger(L, 1);
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)((ms % 1000) * 1000000);
    int64_t result = sys_nanosleep((uint64_t)&ts, 0, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    return push_ok(L);
}

/*
** sys.time()
**
** Returns the current wall-clock time as a floating-point Unix timestamp
** (seconds since 1970-01-01 00:00:00 UTC, with sub-second precision).
*/
static int lsys_time(lua_State* L) {
    struct timespec ts;
    int64_t result = sys_clock_gettime(CLOCK_REALTIME, (uint64_t)&ts, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    lua_pushnumber(L, (lua_Number)ts.tv_sec + (lua_Number)ts.tv_nsec / 1000000000.0);
    return 1;
}

/*
** sys.uptime()
**
** Returns the monotonic time since boot in seconds (float).
** Use this for measuring elapsed durations — it cannot jump backwards.
*/
static int lsys_uptime(lua_State* L) {
    struct timespec ts;
    sys_clock_gettime(CLOCK_MONOTONIC, (uint64_t)&ts, 0, 0, 0, 0);
    lua_pushnumber(L, (lua_Number)ts.tv_sec + (lua_Number)ts.tv_nsec / 1000000000.0);
    return 1;
}

/*
** sys.clock_gettime(clk_id)
**
** Raw clock_gettime interface.
** Returns  sec, nsec  on success, or nil + error_code.
**
** clk_id values are exposed as constants:
**   vespera.CLOCK_REALTIME, vespera.CLOCK_MONOTONIC, vespera.CLOCK_PROCESS_CPUTIME_ID
*/
static int lsys_clock_gettime(lua_State* L) {
    int clk = (int)luaL_checkinteger(L, 1);
    struct timespec ts;
    int64_t result = sys_clock_gettime((uint64_t)clk, (uint64_t)&ts, 0, 0, 0, 0);
    if (result < 0) return push_errno(L, result);
    lua_pushinteger(L, (lua_Integer)ts.tv_sec);
    lua_pushinteger(L, (lua_Integer)ts.tv_nsec);
    return 2;
}

/*
** sys.reboot([mode])
**
** Reboot or power off the system. Does not return on success.
** mode: 0=restart (default), 1=power_off, 2=halt
*/
static int lsys_reboot(lua_State* L) {
    int mode = (int)luaL_optinteger(L, 1, 0);

    uint64_t cmd;
    switch (mode) {
        case 1:
            cmd = 1;
            break; /* REBOOT_POWER_OFF */
        case 2:
            cmd = 2;
            break; /* REBOOT_HALT */
        default:
            cmd = 0;
            break; /* REBOOT_RESTART */
    }

    sys_reboot(0xfee1dead, 0x28121969, cmd, 0, 0, 0);
    /* Should not reach here */
    return push_errno(L, -EIO);
}

/*
** sys.getenv(name)  — get environment variable
** sys.setenv(name, value [, overwrite])
** sys.unsetenv(name)
** sys.environ()     — return copy of the environment as a table
*/
static int lsys_getenv(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    char* val = getenv(name);
    if (val)
        lua_pushstring(L, val);
    else
        lua_pushnil(L);
    return 1;
}

static int lsys_setenv(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* value = luaL_checkstring(L, 2);
    int overwrite = (int)luaL_optinteger(L, 3, 1);
    int result = setenv(name, value, overwrite);
    if (result < 0) return push_errno(L, (int64_t)result);
    return push_ok(L);
}

static int lsys_unsetenv(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int result = unsetenv(name);
    if (result < 0) return push_errno(L, (int64_t)result);
    return push_ok(L);
}

static int lsys_environ(lua_State* L) {
    lua_newtable(L);
    if (!environ) return 1;
    for (int i = 0; environ[i]; i++) {
        lua_pushstring(L, environ[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static const luaL_Reg sys_lib[] = {
    {"sleep", lsys_sleep},
    {"sleep_ms", lsys_sleep_ms},
    {"time", lsys_time},
    {"uptime", lsys_uptime},
    {"clock_gettime", lsys_clock_gettime},
    {"reboot", lsys_reboot},
    {"getenv", lsys_getenv},
    {"setenv", lsys_setenv},
    {"unsetenv", lsys_unsetenv},
    {"environ", lsys_environ},
    {NULL, NULL}
};

/* ============================================================================
** vespera.vbus  — Virtual event bus
** ============================================================================ */

/*
** vbus.subscribe(interface [, member])
**
** Subscribe to vbus signals matching interface/member.
** member = "" or omitted = all members on the interface.
**
** Example:
**   vespera.vbus.subscribe(vespera.VBUS_IFACE_POWER, vespera.VBUS_SIG_BATTERY_CHANGED)
**
** After subscribing, use vbus.recv() or poll(HANDLE_VBUS, POLLIN) to wait.
**
** Returns true on success, or nil + error_code.
*/
static int lvbus_subscribe(lua_State* L) {
    const char* iface = luaL_checkstring(L, 1);
    const char* member = luaL_optstring(L, 2, "");
    int result = vbus_subscribe(iface, member);
    if (result < 0) return push_errno(L, (int64_t)result);
    return push_ok(L);
}

/*
** vbus.unsubscribe()
**
** Remove all subscriptions for the calling realm.
*/
static int lvbus_unsubscribe(lua_State* L) {
    int result = vbus_unsubscribe();
    if (result < 0) return push_errno(L, (int64_t)result);
    return push_ok(L);
}

/*
** vbus.recv()
**
** Read one vbus message from HANDLE_VBUS.
** Returns a table on success:
**   {
**     interface = string,
**     member    = string,
**     payload   = string (raw bytes, may be empty),
**   }
** Returns nil, -EAGAIN if no message is waiting.
** Returns nil, error_code on failure.
*/
static int lvbus_recv(lua_State* L) {
    vbus_header_t hdr;
    char payload[256];
    int result = vbus_recv(&hdr, payload, sizeof(payload));

    if (result == 0) {
        /* Empty — non-blocking */
        lua_pushnil(L);
        lua_pushinteger(L, -EAGAIN);
        return 2;
    }
    if (result < 0) return push_errno(L, (int64_t)result);

    lua_createtable(L, 0, 4);

    lua_pushstring(L, hdr.interface);
    lua_setfield(L, -2, "interface");

    lua_pushstring(L, hdr.member);
    lua_setfield(L, -2, "member");

    lua_pushlstring(L, payload, hdr.payload_size < sizeof(payload) ? hdr.payload_size : sizeof(payload));
    lua_setfield(L, -2, "payload");

    return 1;
}

/*
** vbus.recv_battery()
**
** Returns:
**   { percent, charging, present, critical,
**     remaining_mwh, rate_mw, full_capacity_mwh, index }
*/
static int lvbus_recv_battery(lua_State* L) {
    vbus_header_t hdr;
    vbus_battery_t bat;

    int result = vbus_recv_battery(&hdr, &bat);

    if (result <= 0) {
        lua_pushnil(L);
        lua_pushinteger(L, result == 0 ? -EAGAIN : (lua_Integer)result);
        return 2;
    }

    lua_createtable(L, 0, 8);

    // percent (255 = unknown -> nil)
    if (bat.percent == 255) {
        lua_pushnil(L);
    } else {
        lua_pushinteger(L, (lua_Integer)bat.percent);
    }
    lua_setfield(L, -2, "percent");

    lua_pushboolean(L, bat.charging);
    lua_setfield(L, -2, "charging");

    lua_pushboolean(L, bat.present);
    lua_setfield(L, -2, "present");

    lua_pushboolean(L, bat.critical);
    lua_setfield(L, -2, "critical");

    // remaining_mwh (0xFFFFFFFF = unknown)
    if (bat.remaining_mwh == 0xFFFFFFFF) {
        lua_pushnil(L);
    } else {
        lua_pushinteger(L, (lua_Integer)bat.remaining_mwh);
    }
    lua_setfield(L, -2, "remaining_mwh");

    // rate_mw (0 = unknown)
    if (bat.rate_mw == 0) {
        lua_pushnil(L);
    } else {
        lua_pushinteger(L, (lua_Integer)bat.rate_mw);
    }
    lua_setfield(L, -2, "rate_mw");

    // full_capacity_mwh (0 = unknown?)
    if (bat.full_capacity_mwh == 0) {
        lua_pushnil(L);
    } else {
        lua_pushinteger(L, (lua_Integer)bat.full_capacity_mwh);
    }
    lua_setfield(L, -2, "full_capacity_mwh");

    lua_pushinteger(L, (lua_Integer)bat.index);
    lua_setfield(L, -2, "index");

    return 1;
}

/*
** vbus.recv_ac()
**
** Convenience: receive an AC adapter status update.
** Returns { online = bool } or nil + error.
*/
static int lvbus_recv_ac(lua_State* L) {
    vbus_header_t hdr;
    vbus_ac_t ac;
    int result = vbus_recv_ac(&hdr, &ac);
    if (result <= 0) {
        lua_pushnil(L);
        lua_pushinteger(L, result == 0 ? -EAGAIN : (lua_Integer)result);
        return 2;
    }
    lua_createtable(L, 0, 1);
    lua_pushboolean(L, ac.online);
    lua_setfield(L, -2, "online");
    return 1;
}

static const luaL_Reg vbus_lib[] = {
    {"subscribe", lvbus_subscribe},
    {"unsubscribe", lvbus_unsubscribe},
    {"recv", lvbus_recv},
    {"recv_battery", lvbus_recv_battery},
    {"recv_ac", lvbus_recv_ac},
    {NULL, NULL}
};

/* ============================================================================
** vespera.log  — Logging helpers
** ============================================================================ */

/*
** log.write(message)
**
** Write a message to the kernel log (via HANDLE_STDERR), with newline.
** Returns bytes_written on success, or nil + error_code.
*/
static int llog_write(lua_State* L) {
    size_t len;
    const char* msg = luaL_checklstring(L, 1, &len);
    sys_write(HANDLE_STDERR, (uint64_t)msg, len, 0, 0, 0);
    int64_t r = sys_write(HANDLE_STDERR, (uint64_t)"\n", 1, 0, 0, 0);
    if (r < 0) return push_errno(L, r);
    lua_pushinteger(L, (lua_Integer)(len + 1));
    return 1;
}

/*
** log.writef(fmt, ...)
**
** Formatted variant — delegates to string.format, then calls log.write.
*/
static int llog_writef(lua_State* L) {
    int nargs = lua_gettop(L);
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "format");
    lua_remove(L, -2); /* remove string table */
    for (int i = 1; i <= nargs; i++) lua_pushvalue(L, i);
    lua_call(L, nargs, 1);
    /* Now the formatted string is on top — reuse llog_write */
    return llog_write(L);
}

/*
** log.info / log.warn / log.error
**
** Convenience prefixed variants.
**   log.info("msg")  →  "[INFO]  msg\n"
*/
static int llog_prefixed(lua_State* L, const char* prefix) {
    size_t len;
    const char* msg = luaL_checklstring(L, 1, &len);
    size_t plen = strlen(prefix);

    char* buf = (char*)malloc(plen + len + 2);
    if (!buf) return luaL_error(L, "out of memory");
    memcpy(buf, prefix, plen);
    memcpy(buf + plen, msg, len);
    buf[plen + len] = '\n';
    buf[plen + len + 1] = '\0';

    sys_write(HANDLE_STDERR, (uint64_t)buf, plen + len + 1, 0, 0, 0);
    free(buf);
    return push_ok(L);
}

static int llog_info(lua_State* L) {
    return llog_prefixed(L, "[INFO]  ");
}

static int llog_warn(lua_State* L) {
    return llog_prefixed(L, "[WARN]  ");
}

static int llog_error(lua_State* L) {
    return llog_prefixed(L, "[ERROR] ");
}

static int llog_debug(lua_State* L) {
    return llog_prefixed(L, "[DEBUG] ");
}

static const luaL_Reg log_lib[] = {
    {"write", llog_write},
    {"writef", llog_writef},
    {"info", llog_info},
    {"warn", llog_warn},
    {"error", llog_error},
    {"debug", llog_debug},
    {NULL, NULL}
};

/* ============================================================================
** Constant tables
** ============================================================================ */

typedef struct {
    const char* name;
    lua_Integer value;
} int_const_t;

static const int_const_t vesp_constants[] = {
    /* ── Seek modes ─────────────────────────────────────────────────────── */
    {"SEEK_SET", SEEK_SET},
    {"SEEK_CUR", SEEK_CUR},
    {"SEEK_END", SEEK_END},

    /* ── Standard handles ───────────────────────────────────────────────── */
    {"HANDLE_STDIN", (lua_Integer)HANDLE_STDIN},
    {"HANDLE_STDOUT", (lua_Integer)HANDLE_STDOUT},
    {"HANDLE_STDERR", (lua_Integer)HANDLE_STDERR},
    {"HANDLE_VBUS", (lua_Integer)HANDLE_VBUS},

    /* ── Open flags ─────────────────────────────────────────────────────── */
    {"O_RDONLY", O_RDONLY},
    {"O_WRONLY", O_WRONLY},
    {"O_RDWR", O_RDWR},
    {"O_CREAT", O_CREAT},
    {"O_TRUNC", O_TRUNC},
    {"O_APPEND", O_APPEND},
    {"O_DIRECTORY", O_DIRECTORY},

    /* ── Create flags ───────────────────────────────────────────────────── */
    {"C_FILE", C_FILE},
    {"C_DIR", C_DIR},

    /* ── Poll event flags ───────────────────────────────────────────────── */
    {"POLLIN", POLLIN},
    {"POLLOUT", POLLOUT},
    {"POLLERR", POLLERR},
    {"POLLHUP", POLLHUP},

    /* ── Signals ─────────────────────────────────────────────────────────── */
    {"SIGHUP", SIGHUP},
    {"SIGINT", SIGINT},
    {"SIGQUIT", SIGQUIT},
    {"SIGILL", SIGILL},
    {"SIGTRAP", SIGTRAP},
    {"SIGABRT", SIGABRT},
    {"SIGBUS", SIGBUS},
    {"SIGFPE", SIGFPE},
    {"SIGKILL", SIGKILL},
    {"SIGSEGV", SIGSEGV},
    {"SIGUSR1", SIGUSR1},
    {"SIGUSR2", SIGUSR2},
    {"SIGPIPE", SIGPIPE},
    {"SIGALRM", SIGALRM},
    {"SIGTERM", SIGTERM},
    {"SIGCHLD", SIGCHLD},

    /* ── sigaction flags ────────────────────────────────────────────────── */
    {"SA_RESTART", SA_RESTART},
    {"SA_NODEFER", SA_NODEFER},
    {"SA_RESETHAND", SA_RESETHAND},

    /* ── Clock IDs ──────────────────────────────────────────────────────── */
    {"CLOCK_REALTIME", CLOCK_REALTIME},
    {"CLOCK_MONOTONIC", CLOCK_MONOTONIC},
    {"CLOCK_PROCESS_CPUTIME_ID", CLOCK_PROCESS_CPUTIME_ID},

    /* ── Reboot modes ───────────────────────────────────────────────────── */
    {"REBOOT_RESTART", 0},
    {"REBOOT_POWEROFF", 1},
    {"REBOOT_HALT", 2},

    /* ── Errno codes ─────────────────────────────────────────────────────── */
    {"EPERM", EPERM},
    {"ENOENT", ENOENT},
    {"ESRCH", ESRCH},
    {"EINTR", EINTR},
    {"EIO", EIO},
    {"ENXIO", ENXIO},
    {"ENOEXEC", ENOEXEC},
    {"EBADH", EBADH},
    {"EAGAIN", EAGAIN},
    {"ENOMEM", ENOMEM},
    {"EACCES", EACCES},
    {"EFAULT", EFAULT},
    {"EBUSY", EBUSY},
    {"EEXIST", EEXIST},
    {"ENODEV", ENODEV},
    {"ENOTDIR", ENOTDIR},
    {"EISDIR", EISDIR},
    {"EINVAL", EINVAL},
    {"ENFILE", ENFILE},
    {"EMFILE", EMFILE},
    {"ENOSPC", ENOSPC},
    {"ESPIPE", ESPIPE},
    {"EROFS", EROFS},
    {"EPIPE", EPIPE},
    {"ERANGE", ERANGE},
    {"ENAMETOOLONG", ENAMETOOLONG},
    {"ENOSYS", ENOSYS},
    {"ENOTEMPTY", ENOTEMPTY},
    {"ELOOP", ELOOP},
    {"EOVERFLOW", EOVERFLOW},
    {"EUNKNOWN", EUNKNOWN},
    {"EUNSUPPORTED", EUNSUPPORTED},
    {"EWOULDBLOCK", EWOULDBLOCK},

    /* ── vstat node types (stat.h) ─────────────────────────────────────────── */
    {"VSTAT_TYPE_UNKNOWN", VSTAT_TYPE_UNKNOWN},
    {"VSTAT_TYPE_FILE", VSTAT_TYPE_FILE},
    {"VSTAT_TYPE_DIR", VSTAT_TYPE_DIR},
    {"VSTAT_TYPE_CHARDEV", VSTAT_TYPE_CHARDEV},
    {"VSTAT_TYPE_BLOCKDEV", VSTAT_TYPE_BLOCKDEV},
    {"VSTAT_TYPE_SYMLINK", VSTAT_TYPE_SYMLINK},

    /* ── vstat flags (stat.h) ──────────────────────────────────────────────── */
    {"VSTAT_FLAG_READABLE", VSTAT_FLAG_READABLE},
    {"VSTAT_FLAG_WRITABLE", VSTAT_FLAG_WRITABLE},
    {"VSTAT_FLAG_EXEC", VSTAT_FLAG_EXEC},
    {"VSTAT_FLAG_VIRTUAL", VSTAT_FLAG_VIRTUAL},
    {"VSTAT_FLAG_PERMANENT", VSTAT_FLAG_PERMANENT},

    /* ── dirent types (dirent.h) ───────────────────────────────────────────── */
    {"DT_UNKNOWN", DT_UNKNOWN},
    {"DT_FILE", DT_FILE},
    {"DT_DIR", DT_DIR},
    {"DT_SYMLINK", DT_SYMLINK},
    {"DT_CHARDEV", DT_CHARDEV},
    {"DT_BLOCKDEV", DT_BLOCKDEV},
    {"DT_FIFO", DT_FIFO},
    {"DT_SOCKET", DT_SOCKET},
    {"DT_EXEC", DT_EXEC},

    /* ── mount flags (mount.h) ─────────────────────────────────────────────── */
    {"MS_RDONLY", MS_RDONLY},
    {"MS_NOATIME", MS_NOATIME},
    {"MS_NOEXEC", MS_NOEXEC},
    {"MS_REMOUNT", MS_REMOUNT},

    /* ── capabilities (capabilities.h) ────────────────────────────────────── */
    {"CAP_NONE", (lua_Integer)CAP_NONE},
    {"CAP_READ", (lua_Integer)CAP_READ},
    {"CAP_WRITE", (lua_Integer)CAP_WRITE},
    {"CAP_RW", (lua_Integer)CAP_RW},
    {"CAP_EXECUTE", (lua_Integer)CAP_EXECUTE},
    {"CAP_NETWORK_BIND", (lua_Integer)CAP_NETWORK_BIND},
    {"CAP_UNIT_SPAWN", (lua_Integer)CAP_UNIT_SPAWN},
    {"CAP_DEVICE_ACCESS", (lua_Integer)CAP_DEVICE_ACCESS},
    {"CAP_ALL", (lua_Integer)CAP_ALL},

    /* ── vbus message types (vbus.h) ───────────────────────────────────────── */
    {"VBUS_MSG_SIGNAL", VBUS_MSG_SIGNAL},
    {"VBUS_MSG_CALL", VBUS_MSG_CALL},
    {"VBUS_MSG_RETURN", VBUS_MSG_RETURN},
    {"VBUS_MSG_ERROR", VBUS_MSG_ERROR},

    /* ── vbus subscribe flags (vbus.h) ─────────────────────────────────────── */
    {"VBUS_SUB_WILDCARD", VBUS_SUB_WILDCARD},

    {NULL, 0}
};

/* Helper: register a submodule table.
** Leaves the top-level vespera table on top of the stack. */
static void register_submodule(lua_State* L, const char* name, const luaL_Reg* lib) {
    luaL_newlib(L, lib);
    lua_setfield(L, -2, name);
}

static const luaL_Reg vesp_lib_root[] = {
    {NULL, NULL}
};

LUAMOD_API int luaopen_vespera(lua_State* L) {
    luaL_newlib(L, vesp_lib_root);

    /* Register submodules */
    register_submodule(L, "proc", proc_lib);
    register_submodule(L, "io", io_lib);
    register_submodule(L, "fs", fs_lib);
    register_submodule(L, "ipc", ipc_lib);
    register_submodule(L, "sys", sys_lib);
    register_submodule(L, "vbus", vbus_lib);
    register_submodule(L, "log", log_lib);

    /* Register constants onto the top-level table */
    for (int i = 0; vesp_constants[i].name != NULL; i++) {
        lua_pushinteger(L, vesp_constants[i].value);
        lua_setfield(L, -2, vesp_constants[i].name);
    }

    /* ── vbus string constants (interfaces & signals) ───────────────────── */
#define SET_STR(name, value)   \
    lua_pushliteral(L, value); \
    lua_setfield(L, -2, name)

    SET_STR("VBUS_IFACE_POWER", VBUS_IFACE_POWER);
    SET_STR("VBUS_IFACE_DISPLAY", VBUS_IFACE_DISPLAY);
    SET_STR("VBUS_IFACE_INPUT", VBUS_IFACE_INPUT);
    SET_STR("VBUS_IFACE_STORAGE", VBUS_IFACE_STORAGE);
    SET_STR("VBUS_IFACE_PROC",        VBUS_IFACE_PROC);

    SET_STR("VBUS_SIG_PROC_ORPHANED", VBUS_SIG_PROC_ORPHANED);
    SET_STR("VBUS_SIG_BATTERY_CHANGED", VBUS_SIG_BATTERY_CHANGED);
    SET_STR("VBUS_SIG_AC_CHANGED", VBUS_SIG_AC_CHANGED);
    SET_STR("VBUS_SIG_SLEEP_REQUEST", VBUS_SIG_SLEEP_REQUEST);
    SET_STR("VBUS_SIG_WAKE", VBUS_SIG_WAKE);
    SET_STR("VBUS_SIG_LID_CHANGED", VBUS_SIG_LID_CHANGED);
#undef SET_STR

    /* vespera.version */
    lua_pushliteral(L, "1.0.0");
    lua_setfield(L, -2, "version");

    return 1;
}
