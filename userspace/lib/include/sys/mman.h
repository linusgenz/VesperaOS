// mman.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 27.09.25.
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

#ifndef VESPERAOS_MMAN_H
#define VESPERAOS_MMAN_H

#include <stddef.h>


/**
 * @brief Memory protection flags for mmap
 */
#define PROT_NONE   0x0   ///< No access
#define PROT_READ   0x1   ///< Pages can be read
#define PROT_WRITE  0x2   ///< Pages can be written
#define PROT_EXEC   0x4   ///< Pages can be executed

/**
 * @brief Mapping flags for mmap
 */
#define MAP_SHARED      0x01  ///< Share changes with other processes
#define MAP_PRIVATE     0x02  ///< Changes are private to this process
#define MAP_ANONYMOUS   0x20  ///< Mapping is not backed by a file
#define MAP_FIXED       0x10  ///< Use exactly the address requested (dangerous)
#define MAP_FAILED      ((void*)-1) ///< Return value on failure

/**
 * @brief Map pages into the process address space.
 *
 * @param addr Desired address (may be 0 for automatic placement).
 * @param length Length of mapping in bytes (rounded up to page size).
 * @param prot Protection flags (PROT_READ, PROT_WRITE, PROT_EXEC).
 * @param flags Mapping flags (MAP_PRIVATE, MAP_SHARED, MAP_ANONYMOUS, ...).
 * @param handle File descriptor or handle (if MAP_ANONYMOUS not set, otherwise ignored).
 * @param offset Offset in file (must be multiple of page size).
 *
 * @return On success: pointer to mapped memory.
 *         On error: (void*) -1 with errno set.
 */
void* mmap(void* addr, size_t length, uint64_t prot, uint64_t flags, uint64_t handle, size_t offset);

/**
 * @brief Unmap a previously mapped memory region.
 *
 * @param addr Address returned by mmap().
 * @param length Length of mapping (must match the original length).
 *
 * @return 0 on success, -1 on error (errno set).
 */
int munmap(void* addr, size_t length);

#endif //VESPERAOS_MMAN_H