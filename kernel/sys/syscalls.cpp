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
 * Write to a file descriptor.
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
 * Read from a file descriptor.
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
 * Open a file.
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
 * Close a file descriptor.
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
 * Exit the current process.
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
 * Create a directory.
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
 * Remove a directory.
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
 * Unlink (delete) a file.
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
