// mman.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 26.09.25.
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
 * @brief Advice values for madvise (optional future use)
 */
#define MADV_NORMAL     0 ///< No special treatment
#define MADV_RANDOM     1 ///< Expect random page references
#define MADV_SEQUENTIAL 2 ///< Expect sequential page references
#define MADV_WILLNEED   3 ///< Will need these pages
#define MADV_DONTNEED   4 ///< Don't need these pages

#endif //VESPERAOS_MMAN_H