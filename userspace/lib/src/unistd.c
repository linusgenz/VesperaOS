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

// ---------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------

ssize_t read(int fd, void* buf, size_t count) {
    int64_t ret = sys_read((uint64_t)fd, (uint64_t)(uintptr_t)buf, (uint64_t)count, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (ssize_t)ret;
}

ssize_t write(int fd, const void* buf, size_t count) {
    int64_t ret = sys_write((uint64_t)fd, (uint64_t)(uintptr_t)buf, (uint64_t)count, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (ssize_t)ret;
}

int close(int fd) {
    int64_t ret = sys_close((uint64_t)fd, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
    int64_t ret = sys_seek((uint64_t)fd, (uint64_t)offset, (uint64_t)whence, 0, 0, 0);
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
    int64_t ret = sys_handle_truncate((uint64_t)fd, (uint64_t)length, 0, 0, 0, 0);
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
    (void)fd;
    return 0;
}

int dup(int fd) {
    int64_t ret = sys_dup((uint64_t)fd, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (int)ret;
}

int dup2(int fd, int new_fd) {
    int64_t ret = sys_dup2((uint64_t)fd, (uint64_t)new_fd, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (int)ret;
}

int pipe(int fds[2]) {
    // sys_pipe() takes a pointer to a 2-element handle buffer, matching
    // POSIX pipe(int[2]) layout directly.
    int64_t ret = sys_pipe((uint64_t)(uintptr_t)fds, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int isatty(int fd) {
    // No direct "is this a tty" syscall/ioctl request is defined in
    // sysstd yet. sys_tcgetpgrp() only succeeds on TTY-backed handles, so
    // it's used here as an indirect probe. Revisit if/when a dedicated
    // TCGETS-style ioctl is added.
    int64_t ret = sys_tcgetpgrp((uint64_t)fd, 0, 0, 0, 0, 0);
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
    // sys_nanosleep() takes a millisecond count per sysstd.h's current
    // usage pattern. Interruption isn't distinguished from full
    // completion in the current syscall's return value, so this always
    // reports 0 unslept seconds unless the syscall itself errors.
    int64_t ret = sys_nanosleep((uint64_t)seconds * 1000ULL, 0, 0, 0, 0, 0);
    if (ret < 0) {
        return seconds;
    }
    return 0;
}

int usleep(uint64_t usec) {
    int64_t ret = sys_nanosleep(usec / 1000ULL, 0, 0, 0, 0, 0);
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
    switch (name) {
        case _SC_PAGESIZE:
            return 4096;
        case _SC_NPROCESSORS_ONLN:
            // TODO: no syscall currently exposes CPU topology/count to
            // userspace. Hardcoded to 1 until that exists.
            return 1;
        case _SC_CLK_TCK:
            return 100;
        case _SC_OPEN_MAX:
            // TODO: no syscall exposes the per-realm handle table limit.
            // Using a conservative placeholder.
            return 256;
        default:
            errno = EINVAL;
            return -1;
    }
}