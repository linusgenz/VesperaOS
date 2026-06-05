// shm.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 27.05.26.
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
#ifndef VESPERAOS_SHM_H
#define VESPERAOS_SHM_H

#include <sys/mman.h>

/**
 * @brief Open or create a POSIX shared memory object.
 *
 * @param name Name of the shared memory object (should start with '/').
 * @param oflag Bitwise-OR of O_RDONLY, O_RDWR, O_CREAT, O_EXCL, O_TRUNC.
 * @param mode Permissions mask (optional future use, e.g., 0666).
 *
 * @return On success: A non-negative integer representing the SHM handle.
 * On error: -1 with errno set.
 */
HANDLE shm_open(const char* name, int oflag, uint32_t mode);

/**
 * @brief Remove a POSIX shared memory object name.
 *
 * @param name Name of the shared memory object.
 *
 * @return 0 on success, -1 on error (errno set).
 */
HANDLE shm_unlink(const char* name);

/**
 * @brief Truncate / resize a handle-backed object (required to size SHM).
 *
 * @param handle The SHM handle returned by shm_open.
 * @param length The target size of the shared memory region in bytes.
 *
 * @return 0 on success, -1 on error (errno set).
 */
HANDLE ftruncate(HANDLE handle, size_t length);

#endif  // VESPERAOS_SHM_H
