// syscalls.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 03.08.25.
//
// This file is part of LuminOS.
// 
// LuminOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// LuminOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with LuminOS. If not, see <https://www.gnu.org/licenses/>.

#include "../../arch/x86_64/syscalls/syscall.h"
#include "../include/sys/syscall_numbers.h"
#include "cstddef"

/**
 * @brief Write to a file descriptor.
 *
 * @param fd  The file descriptor. Currently only 1 (stdout) is supported.
 * @param buf Pointer to the data buffer to write.
 * @param size Number of bytes to write.
 * @return Number of bytes written on success, or -1 on error.
 *
 * Possible errors:
 *  - EBADF: Invalid file descriptor.
 *  - EINVAL: Buffer is null or size is zero.
 *  - ENOSYS: Unsupported file descriptor (only stdout allowed).
 */
int64_t sys_write(int fd, const void *buf, size_t size) {
    return syscall(SYSCALL_WRITE, fd, (uint64_t) buf, size);
}

/**
 * @brief Read from a file descriptor.
 *
 * @param fd    File descriptor to read from.
 * @param buf   Buffer to store read data.
 * @param size  Maximum number of bytes to read.
 * @return Number of bytes read on success, or -1 on error.
 *
 * Possible errors:
 *  - EBADF: Invalid or unopened file descriptor.
 *  - EFAULT: Invalid buffer pointer.
 *  - EINVAL: Buffer is null or size is zero.
 */
int64_t sys_read(int fd, void *buf, size_t size) {
    return syscall(SYSCALL_READ, fd, (uint64_t) buf, size);
}

/**
 * @brief Open a file.
 *
 * @param path Absolute path to the file.
 * @return File descriptor (non-negative) on success, or -1 on error.
 *
 * Possible errors:
 *  - ENOENT: File not found.
 *  - EISDIR: Attempted to open a directory as a file.
 *  - ENOMEM: Failed to allocate file descriptor.
 *  - EINVAL: Invalid path.
 */
int64_t sys_open(const char *path) {
    return syscall(SYSCALL_OPEN, (uint64_t) path);
}

/**
 * @brief Close a file descriptor.
 *
 * @param fd File descriptor to close.
 * @return 0 on success, or -1 on error.
 *
 * Possible errors:
 *  - EBADF: Invalid file descriptor.
 */
int64_t sys_close(int fd) {
    return syscall(SYSCALL_CLOSE, fd);
}

/**
 * @brief Exit the current process.
 *
 * @param code Exit status code.
 * @return Does not return.
 *
 * Notes:
 *  - This will terminate the current userspace thread/process.
 */
int64_t sys_exit(int code) {
    return syscall(SYSCALL_EXIT, code);
}

/**
 * @brief Create a file.
 *
 * @param path Absolute path of the file to create.
 * @return 0 on success, or negative errno on failure.
 *
 * Possible errors:
 *  - EEXIST: File already exists.
 *  - EINVAL: Invalid path or parent does not exist.
 *  - ENOMEM: Internal allocation failure.
 */
int64_t sys_create(const char *path) {
    return syscall(SYSCALL_CREATE, (uint64_t) path);
}

/**
 * @brief Rename a file or directory.
 *
 * Attempts to rename the file or directory specified by `old_path` to `new_path`.
 * If `new_path` exists, behavior is implementation-defined — for LuminOS, it may
 * fail or overwrite non-directory files depending on later implementation.
 *
 * This call works for both regular files and directories.
 *
 * @param old_path The existing path of the file or directory to rename.
 * @param new_path The new desired path for the file or directory.
 * @return On success, returns 0.
 *         On failure, returns -1 and sets errno (or returns a negative error code).
 *
 * Error codes:
 *  - `ENOENT`     if the `old_path` does not exist.
 *  - `EEXIST`     if `new_path` already exists and cannot be replaced.
 *  - `ENOTDIR`    if a path component in either path is not a directory.
 *  - `EINVAL`     if either path is invalid.
 *  - `EROFS`      if the filesystem is read-only.
 *  - `ENOTEMPTY`  if renaming a directory over a non-empty directory.
 *  - `EACCES`     if permission is denied.
 *  - `ENOSYS`     if the operation is not supported by the filesystem.
 */
int64_t sys_rename(const char *old_path, const char *new_path) {
    return syscall(SYSCALL_RENAME, (uint64_t) old_path, (uint64_t) new_path);
}

/**
 * @brief Create a directory.
 *
 * @param path Absolute path of the directory to create.
 * @return 0 on success, or negative errno on failure.
 *
 * Possible errors:
 *  - EEXIST: Directory already exists.
 *  - EINVAL: Invalid path or parent does not exist.
 *  - ENOMEM: Internal allocation failure.
 */
int64_t sys_mkdir(const char *path) {
    return syscall(SYSCALL_MKDIR, (uint64_t) path);
}

/**
 * @brief Remove a directory.
 *
 * @param path Absolute path of the directory to remove.
 * @return 0 on success, or negative errno on failure.
 *
 * Possible errors:
 *  - ENOTEMPTY: Directory is not empty.
 *  - EINVAL: Path is invalid or not a directory.
 *  - ENOENT: Directory not found.
 *  - EPERM: Attempt to remove a mount point or protected directory.
 */
int64_t sys_rmdir(const char *path) {
    return syscall(SYSCALL_RMDIR, (uint64_t) path);
}

/**
 * @brief Unlink (delete) a file.
 *
 * @param path Absolute path of the file to delete.
 * @return 0 on success, or negative errno on failure.
 *
 * Possible errors:
 *  - EISDIR: Path refers to a directory, not a file.
 *  - ENOENT: File not found.
 *  - EPERM: Cannot delete protected or open file.
 */
int64_t sys_unlink(const char *path) {
    return syscall(SYSCALL_UNLINK, (uint64_t) path);
}
