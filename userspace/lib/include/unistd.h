// unistd.h
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

#ifndef UNISTD_H
#define UNISTD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------
// POSIX standard constants
// ---------------------------------------------------------------------

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

// Standard handle IDs. VesperaOS handle IDs 0/1/2 are reserved for
// stdio by the spawn/exec path, matching POSIX fd conventions.
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// sysconf() names
#define _SC_PAGESIZE     1
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_NPROCESSORS_ONLN 2
#define _SC_NPROCESSORS_CONF _SC_NPROCESSORS_ONLN
#define _SC_CLK_TCK      3
#define _SC_OPEN_MAX     4
#define _SC_PHYS_PAGES   85

// access() mode bits
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

// ---------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef int64_t ssize_t;
#endif

#ifndef _PID_T_DEFINED
#define _PID_T_DEFINED
typedef int pid_t;
#endif

#ifndef _OFF_T_DEFINED
#define _OFF_T_DEFINED
typedef int64_t off_t;
#endif

#ifndef _UID_T_DEFINED
#define _UID_T_DEFINED
typedef uint32_t uid_t;
#endif

#ifndef _GID_T_DEFINED
#define _GID_T_DEFINED
typedef uint32_t gid_t;
#endif

#ifndef _INTPTR_T_DEFINED
#define _INTPTR_T_DEFINED
#endif

// ---------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------

/**
 * @brief Read up to `count` bytes from `fd` into `buf`.
 * @return Number of bytes read, 0 on EOF, or -1 on error (errno set).
 */
ssize_t read(int fd, void* buf, size_t count);

/**
 * @brief Write up to `count` bytes from `buf` to `fd`.
 * @return Number of bytes written, or -1 on error (errno set).
 */
ssize_t write(int fd, const void* buf, size_t count);

/**
 * @brief Close a file descriptor / handle.
 * @return 0 on success, -1 on error (errno set).
 */
int close(int fd);

/**
 * @brief Reposition the file offset of `fd`.
 * @return New absolute offset on success, or (off_t)-1 on error (errno set).
 */
off_t lseek(int fd, off_t offset, int whence);

/**
 * @brief Remove a file.
 * @return 0 on success, -1 on error (errno set).
 */
int unlink(const char* path);

/**
 * @brief Remove an empty directory.
 * @return 0 on success, -1 on error (errno set).
 */
int rmdir(const char* path);

/**
 * @brief Check accessibility of a file.
 *
 * @param mode Bitwise OR of F_OK, R_OK, W_OK, X_OK.
 * @return 0 if accessible, -1 on error (errno set).
 *
 * @todo TODO
 */
int access(const char* path, int mode);

/**
 * @brief Truncate or extend a file referenced by an open handle.
 * @return 0 on success, -1 on error (errno set).
 */
int ftruncate(int fd, off_t length);

/**
 * @brief Flush a handle to backing storage.
 *
 * @note VesperaOS currently has no dedicated flush/sync syscall; this is a
 *       no-op that always succeeds. Revisit once the VFS exposes one.
 */
int fsync(int fd);

/**
 * @brief Duplicate a file descriptor.
 * @return New descriptor on success, -1 on error (errno set).
 */
int dup(int fd);

/**
 * @brief Duplicate a file descriptor onto a specific target descriptor.
 * @return `new_fd` on success, -1 on error (errno set).
 */
int dup2(int fd, int new_fd);

/**
 * @brief Create a pipe.
 *
 * @param fds fds[0] receives the read end, fds[1] the write end.
 * @return 0 on success, -1 on error (errno set).
 */
int pipe(int fds[2]);

/**
 * @brief Test whether `fd` refers to a terminal device.
 * @return 1 if `fd` is a TTY, 0 otherwise (including on error; errno is
 *         set to ENOTTY in that case per POSIX).
 */
int isatty(int fd);

// ---------------------------------------------------------------------
// Filesystem / working directory
// ---------------------------------------------------------------------

/**
 * @brief Get the current working directory.
 * @return `buf` on success, or NULL on error (errno set, e.g. ERANGE if
 *         `size` is too small).
 */
char* getcwd(char* buf, size_t size);

/**
 * @brief Change the current working directory.
 * @return 0 on success, -1 on error (errno set).
 */
int chdir(const char* path);

// ---------------------------------------------------------------------
// Process / identity
// ---------------------------------------------------------------------

/**
 * @brief Get the calling realm's unit ID.
 *
 * @note VesperaOS has no separate "process ID" concept distinct from
 *       realm/unit IDs; this maps to the current realm's ID and is the
 *       closest equivalent to POSIX getpid().
 */
pid_t getpid(void);

uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);

int setuid(uid_t uid);
int setgid(gid_t gid);

/**
 * @brief Terminate the calling unit.
 *
 * Does not return.
 */
_Noreturn void _exit(int status);

// ---------------------------------------------------------------------
// Sleeping
// ---------------------------------------------------------------------

/**
 * @brief Sleep for (at least) `seconds` seconds.
 * @return 0 if the full duration elapsed, or the number of unslept
 *         seconds if interrupted early.
 */
unsigned int sleep(unsigned int seconds);

/**
 * @brief Sleep for (at least) `usec` microseconds.
 * @return 0 on success, -1 on error (errno set).
 */
int usleep(uint64_t usec);

// ---------------------------------------------------------------------
// System configuration
// ---------------------------------------------------------------------

/**
 * @brief Query a runtime system limit or option (see _SC_* above).
 * @return The requested value, or -1 if unrecognized (errno set to EINVAL).
 *
 * @note Only a small subset of POSIX `name` values is implemented; others
 *       return -1/EINVAL rather than a guessed value.
 */
int64_t sysconf(int name);

#ifdef __cplusplus
}
#endif

#endif // UNISTD_H