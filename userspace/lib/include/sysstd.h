// sysstd.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 22.09.25.
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


#ifndef SYSSTD_H
#define SYSSTD_H

#include <stdint.h>

static int64_t syscall(uint64_t num,
                uint64_t arg0,
                uint64_t arg1,
                uint64_t arg2,
                uint64_t arg3,
                uint64_t arg4,
                uint64_t arg5);

/**
 * @brief Close a handle.
 *
 * @param hid Handle ID to close.
 * @return 0 on success, or -EBADH on invalid handle.
 */
int64_t sys_close(uint64_t hid, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Create a file.
 *
 * @param path Path of the file to create.
 * @return 0 on success, or negative error code (e.g., -EEXIST, -EINVAL).
 */
int64_t sys_create(uint64_t path_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Terminate the current unit.
 *
 * @param code Exit code.
 * @return This function does not return; halts the unit.
 */
int64_t sys_exit(uint64_t code, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Perform an ioctl operation on a device handle.
 *
 * @param hid Handle ID.
 * @param request Device-specific request code.
 * @param arg Pointer to request argument.
 * @return Device-dependent result, or negative error code.
 */
int64_t sys_ioctl(uint64_t hid, uint64_t request, uint64_t arg, uint64_t, uint64_t, uint64_t);

/**
 * @brief Create a directory.
 *
 * @param path Directory path.
 * @return 0 on success, or negative error code (e.g., -EEXIST, -EINVAL).
 */
int64_t sys_mkdir(uint64_t path_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Open a file or device.
 *
 * @param path Path of the file/device.
 * @param flags Open flags (O_RDONLY, O_WRONLY, O_RDWR).
 * @return Handle ID on success, or negative error code.
 */
int64_t sys_open(uint64_t path_ptr, uint64_t flags, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Read from a handle into a buffer.
 *
 * @param hid Handle ID.
 * @param buf Buffer pointer.
 * @param count Number of bytes to read.
 * @return Number of bytes read, or negative error code.
 */
int64_t sys_read(uint64_t hid, uint64_t buf_ptr, uint64_t count, uint64_t, uint64_t, uint64_t);

/**
 * @brief Reboot or power off the system.
 *
 * @param magic1 First magic number (must match REBOOT_MAGIC1).
 * @param magic2 Second magic number (must match REBOOT_MAGIC2).
 * @param cmd Reboot command (REBOOT_RESTART, REBOOT_POWER_OFF).
 * @return 0 on success, -1 on failure.
 */
int64_t sys_reboot(uint64_t magic1, uint64_t magic2, uint64_t cmd, uint64_t, uint64_t, uint64_t);

/**
 * @brief Rename a file or directory.
 *
 * @param oldPath Old path.
 * @param newPath New path.
 * @return 0 on success, or negative error code.
 */
int64_t sys_rename(uint64_t oldPath_ptr, uint64_t newPath_ptr, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Remove a directory.
 *
 * @param path Directory path.
 * @return 0 on success, or negative error code (e.g., -ENOTEMPTY).
 */
int64_t sys_rmdir(uint64_t path_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Sleep the current unit for a number of milliseconds.
 *
 * @param ms Milliseconds to sleep.
 * @return 0 on success.
 */
int64_t sys_sleep(uint64_t ms, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Spawn a new realm/unit.
 *
 * @param path Path to binary.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return New realm ID on success, or negative error code.
 */
int64_t sys_spawn(uint64_t path_ptr, uint64_t argc, uint64_t argv_ptr, uint64_t, uint64_t, uint64_t);

/**
 * @brief Unlink (delete) a file.
 *
 * @param path Path to the file.
 * @return 0 on success, or negative error code (e.g., -ENOENT).
 */
int64_t sys_unlink(uint64_t path_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Write data to a handle.
 *
 * @param hid Handle ID.
 * @param buf Buffer pointer.
 * @param count Number of bytes to write.
 * @return Number of bytes written, or negative error code.
 */
int64_t sys_write(uint64_t hid, uint64_t buf_ptr, uint64_t count, uint64_t, uint64_t, uint64_t);


#endif //SYSSTD_H
