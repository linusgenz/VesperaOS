// fcntl.h
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
//
// POSIX fcntl.h compatibility shim, implemented on top of sysstd.h and
// vespera/fflags.h. Exists primarily to satisfy third-party ports (e.g.
// Mesa, zlib, expat) that expect a standard <fcntl.h>. Only the subset
// of POSIX fcntl semantics that has a real equivalent in the VesperaOS
// syscall surface is implemented; the rest is stubbed and documented
// below.

#ifndef FCNTL_H
#define FCNTL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <vespera/fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define C_FILE 0x00 /**< The target should be a file */
#define C_DIR 0x01  /**< The target should be a directory */

// ---------------------------------------------------------------------
// Open flags
// ---------------------------------------------------------------------
// O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_EXCL, O_TRUNC, O_APPEND, and
// O_DIRECTORY all come from <vespera/fflags.h>, included above, and are
// re-exported here as-is so ported code that only knows about
// <fcntl.h> picks them up transparently.

// Flags with no VesperaOS syscall-level backing yet. Defined so ported
// code compiles; passing them to open() is accepted but currently has
// no effect (see open() note below).
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x0800 /**< No syscall-level support yet; accepted, currently a no-op. */
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0x80000 /**< No exec-time handle table semantics yet; accepted, currently a no-op. */
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0x20000 /**< No symlink support in the VFS yet; accepted, currently a no-op. */
#endif

// ---------------------------------------------------------------------
// fcntl() commands
// ---------------------------------------------------------------------

#define F_DUPFD 0  /**< Duplicate handle. Backed by sys_dup(); see fcntl() note on F_DUPFD's minimum-fd semantics. */
#define F_GETFD 1  /**< Get handle flags. Not tracked by VesperaOS yet; always returns 0. */
#define F_SETFD 2  /**< Set handle flags. Not tracked by VesperaOS yet; accepted as a no-op. */
#define F_GETFL 3  /**< Get file status flags. Not tracked per-handle yet; returns -1/ENOSYS. */
#define F_SETFL 4  /**< Set file status flags. Not tracked per-handle yet; returns -1/ENOSYS. */
#define F_GETLK 5  /**< Get record lock. No locking support; returns -1/ENOSYS. */
#define F_SETLK 6  /**< Set record lock. No locking support; returns -1/ENOSYS. */
#define F_SETLKW 7 /**< Set record lock (blocking). No locking support; returns -1/ENOSYS. */

// FD_CLOEXEC bit for F_GETFD/F_SETFD. Defined for source compatibility;
// see F_GETFD/F_SETFD notes above.
#define FD_CLOEXEC 1

// ---------------------------------------------------------------------
// Functions
// ---------------------------------------------------------------------

/**
 * @brief Open (or create) a file, returning a raw handle.
 *
 * @param path Path to the file, null-terminated.
 * @param flags Bitwise OR of O_* flags (see <vespera/fflags.h> and the
 *              additional O_* flags defined above).
 * @param ... Optional `mode_t mode` argument, accepted for POSIX source
 *            compatibility but currently discarded: VesperaOS's
 *            sys_open() takes no permission-mode argument, so files are
 *            created with whatever default mode the kernel assigns.
 * @return Non-negative handle ID on success, or -1 on error (errno set).
 *
 * @see close()
 */
int open(const char* path, int flags, ...);

/**
 * @brief Create a new file, equivalent to `open(path, O_CREAT | O_WRONLY | O_TRUNC)`.
 *
 * @param path Path to the file.
 * @return Non-negative handle ID on success, or -1 on error (errno set).
 *
 * @see open()
 */
int creat(const char* path);

/**
 * @brief Perform a control operation on an open file handle.
 *
 * @param fd Handle ID, as returned by open().
 * @param cmd One of the F_* command constants above.
 * @param ... Command-specific argument.
 *
 *   - F_DUPFD: `int min_fd` argument. Duplicates `fd` via sys_dup().
 *     @note VesperaOS's sys_dup() has no "return the lowest available
 *     handle >= min_fd" semantics like POSIX F_DUPFD requires; it simply
 *     returns whatever handle ID the kernel's handle-table allocator
 *     hands back next. `min_fd` is currently ignored. Revisit if/when
 *     sys_dup() gains a minimum-handle parameter.
 *   - F_GETFD / F_SETFD: accepted as no-ops (see constants above).
 *   - F_GETFL / F_SETFL / F_GETLK / F_SETLK / F_SETLKW: not implemented;
 *     return -1 with errno set to ENOSYS.
 *
 * @return Command-specific result on success, or -1 on error (errno set).
 */
int fcntl(int fd, int cmd, ...);

#ifdef __cplusplus
}
#endif

#endif // FCNTL_H