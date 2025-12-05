// stdlib.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 23.09.25.
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

#ifndef VESPERAOS_STDLIB_H
#define VESPERAOS_STDLIB_H
#include <stddef.h>
#include <sysstd.h>
#include <sys/mman.h>

typedef int64_t FILE_HANDLE;

#ifdef __cplusplus
extern "C" {
#endif

extern char **environ;
extern FILE_HANDLE stdin;
extern FILE_HANDLE stdout;
extern FILE_HANDLE stderr;

#ifdef __cplusplus
}
#endif

typedef long int ssize_t;

/**
 * @brief Get the value of an environment variable.
 *
 * @param name Variable name (null-terminated string).
 * @return Pointer to the value string, or NULL if not found.
 */
char* getenv(const char* name);

/**
 * @brief Set an environment variable.
 *
 * @param name Variable name.
 * @param value Value to set.
 * @param overwrite If 0, existing variables are not overwritten.
 * @return 0 on success, -1 on failure.
 */
int setenv(const char *name, const char *value, int overwrite);

/**
 * @brief Unset an environment variable.
 *
 * @param name Variable name.
 * @return 0 on success, -1 if not found.
 */
int unsetenv(const char *name);

/**
 * @brief Allocates a block of memory of the specified size.
 *
 * The memory is allocated using the kernel's memory mapping syscall (mmap).
 * This is a very basic allocator and does not reuse freed blocks or implement
 * advanced heap management.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory on success,
 *         or NULL if allocation failed.
 */
void *malloc(size_t size);

/**
 * @brief Releases a block of memory previously allocated by malloc().
 *
 * The memory is unmapped using the kernel's sys_munmap.
 * Since this allocator uses mmap per allocation, the entire block is released.
 *
 * @param ptr Pointer to the memory block to free. If NULL, no action is taken.
 */
void free(void *ptr);

/**
 * @brief Reallocates a memory block to a new size.
 *
 * A new block is allocated with malloc(), the old contents are copied,
 * and the old block is freed.
 *
 * @param ptr Pointer to the memory block to resize (may be NULL).
 * @param new_size New size in bytes.
 * @return Pointer to the new memory block on success,
 *         or NULL if allocation failed (old block is not freed in this case).
 */
void *realloc(void *ptr, size_t new_size);

/**
 * @brief Allocates zero-initialized memory for an array.
 *
 * Equivalent to malloc(nmemb * size) followed by memset to zero.
 *
 * @param nmemb Number of array elements.
 * @param size Size of each element.
 * @return Pointer to zero-initialized memory, or NULL on failure.
 */
void *calloc(size_t nmemb, size_t size);

#endif //VESPERAOS_STDLIB_H
