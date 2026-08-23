// dirent.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 29.09.25.
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

#ifndef _DIRENT_H
#define _DIRENT_H

#include <vespera/dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque directory stream handle.
 *
 * The layout is deliberately not exposed in this header (POSIX only
 * requires that DIR be an incomplete/opaque type to callers). See
 * internal/dirstream.h for the actual definition.
 */
typedef struct __dirstream DIR;

/**
 * @brief Open a directory stream for `path`.
 *
 * @param path Path of the directory to open.
 * @return A DIR* stream on success, or NULL on error (errno set).
 *
 * Errors (errno):
 *   ENOENT  - path does not exist
 *   ENOTDIR - path exists but is not a directory
 *   EACCES  - insufficient capabilities
 *   EMFILE  - fd table exhausted (see FD_TABLE_MAX)
 *   ENOMEM  - allocation of the DIR stream failed
 */
DIR* opendir(const char* path);

/**
 * @brief Open a directory stream from an already-open directory fd.
 *
 * Takes ownership of `fd` (consumed by closedir() or on failure, exactly
 * like fdopen() does for a FILE*).
 *
 * @param fd An fd previously obtained by open(path, O_DIRECTORY | ...).
 * @return A DIR* stream on success, or NULL on error (errno set).
 */
DIR* fdopendir(int fd);

/**
 * @brief Read the next directory entry from `dirp`.
 *
 * The returned pointer refers to storage owned by `dirp` and is only
 * valid until the next call to readdir() or closedir() on the same
 * stream; it must not be freed by the caller.
 *
 * @param dirp Open directory stream (see opendir()).
 * @return Pointer to the next dirent_t, or NULL at end-of-directory or
 *         on error. To distinguish the two, errno is left unchanged by
 *         readdir() on end-of-directory and is set to a nonzero value
 *         on error (check errno == 0 before the call if this matters).
 */
dirent_t* readdir(DIR* dirp);

/**
 * @brief Close a directory stream, releasing the underlying handle.
 *
 * @param dirp Open directory stream.
 * @return 0 on success, -1 on error (errno set).
 */
int closedir(DIR* dirp);

/**
 * @brief Rewind a directory stream back to the first entry.
 *
 * @note VesperaOS's sys_readdir() has no native seek/rewind primitive
 *       yet. This is implemented by closing and re-opening the
 *       underlying handle at the same path. Any fd obtained via
 *       fdopendir() therefore cannot be rewound (see rewinddir()
 *       implementation note) and rewinddir() is a no-op with errno set
 *       to EBADF in that case.
 */
void rewinddir(DIR* dirp);

/**
 * @brief Return a file descriptor associated with a directory stream.
 *
 * @param dirp Open directory stream.
 * @return The underlying fd. Do not close() it directly while `dirp`
 *         is still in use; use closedir() instead.
 */
int dirfd(DIR* dirp);

#ifdef __cplusplus
}
#endif


#endif //_DIRENT_H