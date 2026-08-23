// unistd.c
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 14.08.26.
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

#include <unistd.h>

#include <errno.h>
#include <sysstd.h>
#include <sys/sysinfo.h>

#include "sched.h"
#include "internal/fd_table.h"
#include <vespera/handles.h>

#include "sys/time.h"


// ---------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------

ssize_t read(int fd, void* buf, size_t count) {
    FILE_HANDLE handle = fd_table_get(fd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    int64_t ret = sys_read((uint64_t)handle, (uint64_t)(uintptr_t)buf, (uint64_t)count, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (ssize_t)ret;
}

ssize_t write(int fd, const void* buf, size_t count) {
    FILE_HANDLE handle = fd_table_get(fd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    int64_t ret = sys_write((uint64_t)handle, (uint64_t)(uintptr_t)buf, (uint64_t)count, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (ssize_t)ret;
}

int close(int fd) {
    FILE_HANDLE handle = fd_table_get(fd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    int64_t ret = sys_close((uint64_t)handle, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    // Only drop the fd slot once the underlying handle is actually
    // closed — if sys_close() fails, leave the mapping intact so the
    // caller can inspect/retry rather than leaking a dangling fd that
    // silently resolves to nothing.
    fd_table_remove(fd);
    return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
    FILE_HANDLE handle = fd_table_get(fd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return (off_t)-1;
    }

    int64_t ret = sys_seek((uint64_t)handle, (uint64_t)offset, (uint64_t)whence, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return (off_t)-1;
    }
    return (off_t)ret;
}

int unlink(const char* path) {
    int64_t ret = sys_unlink((uint64_t)(uintptr_t)path, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int rmdir(const char* path) {
    int64_t ret = sys_rmdir((uint64_t)(uintptr_t)path, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int access(const char* path, int mode) {
    // VesperaOS has no dedicated access-check syscall yet. We approximate
    // it via sys_stat(): if the stat call succeeds the path exists and is
    // reachable, which is treated as satisfying F_OK/R_OK/W_OK. X_OK is
    // not checked against real permission bits (see header note).
    (void)mode;

    unsigned char stat_buf[128]; // oversized scratch buffer; only existence matters here
    int64_t ret = sys_stat((uint64_t)(uintptr_t)path, (uint64_t)(uintptr_t)stat_buf, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int ftruncate(int fd, off_t length) {
    FILE_HANDLE handle = fd_table_get(fd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    int64_t ret = sys_handle_truncate((uint64_t)handle, (uint64_t)length, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int fsync(int fd) {
    // No-op: VesperaOS has no explicit flush/sync syscall exposed to
    // userspace yet. Writes go through sys_write(); there is currently
    // nothing to flush from here. See header note.
    //
    // Still validated against the fd table so callers get EBADH on a
    // bogus fd rather than a silent success.
    if (!fd_table_valid(fd)) {
        errno = EBADH;
        return -1;
    }
    return 0;
}

void sync(void) {
    // todo
}

int dup(int fd) {
    FILE_HANDLE handle = fd_table_get(fd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    int64_t ret = sys_dup((uint64_t)handle, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    int new_fd = fd_table_insert((FILE_HANDLE)ret);
    if (new_fd < 0) {
        sys_close((uint64_t)ret, 0, 0, 0, 0, 0);
        return -1;
    }
    return new_fd;
}

int dup2(int fd, int new_fd) {
    FILE_HANDLE handle = fd_table_get(fd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    if (new_fd < 0 || new_fd >= FD_TABLE_MAX) {
        errno = EBADH;
        return -1;
    }

    if (fd == new_fd) {
        return new_fd;
    }

    FILE_HANDLE dup_handle;
    int64_t ret = sys_dup((uint64_t)handle, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    dup_handle = (FILE_HANDLE)ret;

    FILE_HANDLE old_handle = fd_table_get(new_fd);
    if (old_handle != INVALID_HANDLE) {
        sys_close((uint64_t)old_handle, 0, 0, 0, 0, 0);
    }

    if (fd_table_insert_at(new_fd, dup_handle) < 0) {
        sys_close((uint64_t)dup_handle, 0, 0, 0, 0, 0);
        return -1;
    }
    return new_fd;
}

int pipe(int fds[2]) {
    FILE_HANDLE native_fds[2];
    int64_t ret = sys_pipe((uint64_t)(uintptr_t)native_fds, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    int read_fd = fd_table_insert(native_fds[0]);
    if (read_fd < 0) {
        sys_close((uint64_t)native_fds[0], 0, 0, 0, 0, 0);
        sys_close((uint64_t)native_fds[1], 0, 0, 0, 0, 0);
        return -1;
    }

    int write_fd = fd_table_insert(native_fds[1]);
    if (write_fd < 0) {
        // Roll back the read end too so we don't leak a half-open pipe
        // into the caller's fd space.
        fd_table_remove(read_fd);
        sys_close((uint64_t)native_fds[0], 0, 0, 0, 0, 0);
        sys_close((uint64_t)native_fds[1], 0, 0, 0, 0, 0);
        return -1;
    }

    fds[0] = read_fd;
    fds[1] = write_fd;
    return 0;
}

int isatty(int fd) {
    // No direct "is this a tty" syscall/ioctl request is defined in
    // sysstd yet. sys_tcgetpgrp() only succeeds on TTY-backed handles, so
    // it's used here as an indirect probe. Revisit if/when a dedicated
    // TCGETS-style ioctl is added.
    FILE_HANDLE handle = fd_table_get(fd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return 0;
    }

    int64_t ret = sys_tcgetpgrp((uint64_t)handle, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = ENOTTY;
        return 0;
    }
    return 1;
}

// ---------------------------------------------------------------------
// Filesystem / working directory
// ---------------------------------------------------------------------

char* getcwd(char* buf, size_t size) {
    int64_t ret = sys_getcwd((uint64_t)(uintptr_t)buf, (uint64_t)size, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return (char*)0;
    }
    return buf;
}

int chdir(const char* path) {
    int64_t ret = sys_chdir((uint64_t)(uintptr_t)path, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

// ---------------------------------------------------------------------
// Process / identity
// ---------------------------------------------------------------------

pid_t getpid(void) {
    // VesperaOS has no separate pid concept; realm ID is the closest
    // analogue and is what sys_get_unid() reports for the caller.
    int64_t ret = sys_get_unid(0, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return (pid_t)-1;
    }
    return (pid_t)ret;
}

uid_t getuid(void) {
    return (uid_t)sys_getuid(0, 0, 0, 0, 0, 0);
}

uid_t geteuid(void) {
    return (uid_t)sys_geteuid(0, 0, 0, 0, 0, 0);
}

gid_t getgid(void) {
    return (gid_t)sys_getgid(0, 0, 0, 0, 0, 0);
}

gid_t getegid(void) {
    return (gid_t)sys_getegid(0, 0, 0, 0, 0, 0);
}

int setuid(uid_t uid) {
    int64_t ret = sys_setuid((uint64_t)uid, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int setgid(gid_t gid) {
    int64_t ret = sys_setgid((uint64_t)gid, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

_Noreturn void _exit(int status) {
    sys_exit_group((uint64_t)status, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

// ---------------------------------------------------------------------
// Sleeping
// ---------------------------------------------------------------------

unsigned int sleep(unsigned int seconds) {
    timespec_t req;
    req.tv_sec  = (int64_t)seconds;
    req.tv_nsec = 0;

    int64_t ret = sys_nanosleep((uint64_t)&req, 0, 0, 0, 0, 0);

    if (ret < 0) {
        errno = (int)(-ret);
        return seconds;
    }

    return 0;
}

int usleep(uint64_t usec) {
    timespec_t req;
    req.tv_sec  = (int64_t)(usec / 1000000ULL);
    req.tv_nsec = (int64_t)((usec % 1000000ULL) * 1000ULL);

    int64_t ret = sys_nanosleep((uint64_t)&req, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int chown(const char *path, uid_t owner, gid_t group) {
    int64_t ret = sys_chown((uint64_t)path, (uint64_t)owner, (uint64_t)group, 0, 0, 0);

    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

// ---------------------------------------------------------------------
// System configuration
// ---------------------------------------------------------------------

int64_t sysconf(int name) {
    struct sysinfo info;

    switch (name) {
        case _SC_PAGESIZE:
            return 4096;
        case _SC_NPROCESSORS_ONLN: {
            cpu_set_t set;
            CPU_ZERO(&set);

            if (sched_getaffinity(0, sizeof(cpu_set_t), &set) == 0) {
                int count = CPU_COUNT(&set);
                if (count > 0) {
                    return count;
                }
            }

            return 1;
        }
        case _SC_CLK_TCK:
            return 100;
        case _SC_OPEN_MAX:
            // Matches FD_TABLE_MAX in internal/fd_table.h — the fd
            // table is the actual limiting resource now, not a
            // per-realm handle table cap.
            return FD_TABLE_MAX;
        case _SC_PHYS_PAGES:
            if (sysinfo(&info) < 0) {
                return -1;
            }

            return (long)((info.totalram * info.mem_unit) / 4096);
        default:
            errno = EINVAL;
            return -1;
    }
}

int getpagesize(void) {
    return sysconf(_SC_PAGESIZE);
}