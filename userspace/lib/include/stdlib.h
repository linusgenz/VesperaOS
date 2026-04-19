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

typedef uint64_t FILE_HANDLE;
extern int errno;

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#ifdef __cplusplus
extern "C" {

#endif

/**
 * @brief Array of environment variables in "NAME=VALUE" format.
 *
 * The array is NULL-terminated. Should not be modified directly;
 * use setenv(), unsetenv(), or putenv() instead.
 *
 * @see setenv()
 * @see unsetenv()
 * @see putenv()
 */
extern char** environ;

typedef long int ssize_t;

/**
 * @brief Get the value of an environment variable.
 *
 * Searches the environment list for a variable with the given name and
 * returns a pointer to its value (the part after the '=' character).
 * The returned pointer points to internal memory and must not be modified.
 *
 * @param name Variable name (null-terminated string, must not contain '=').
 * @return Pointer to the value string, or @c NULL if not found.
 *
 * @see setenv()
 * @see unsetenv()
 */
char* getenv(const char* name);

/**
 * @brief Set or modify an environment variable.
 *
 * Adds a new environment variable or modifies an existing one. The function
 * makes internal copies of both the name and value strings.
 *
 * @param name Variable name (must not contain '=').
 * @param value Value to set (null-terminated string).
 * @param overwrite If @c 0, existing variables are not modified. If non-zero, overwrite.
 * @return @c 0 on success, @c -1 on failure.
 *
 * @see getenv()
 * @see unsetenv()
 */
int setenv(const char* name, const char* value, int overwrite);

/**
 * @brief Remove an environment variable.
 *
 * Removes the variable with the given name from the environment.
 * Returns @c 0 even if the variable doesn't exist (POSIX behavior).
 *
 * @param name Variable name (must not contain '=').
 * @return @c 0 on success, @c -1 on failure (invalid parameters).
 *
 * @see getenv()
 * @see setenv()
 */
int unsetenv(const char* name);

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
void* malloc(size_t size);

/**
 * @brief Releases a block of memory previously allocated by malloc().
 *
 * The memory is unmapped using the kernel's sys_munmap.
 * Since this allocator uses mmap per allocation, the entire block is released.
 *
 * @param ptr Pointer to the memory block to free. If NULL, no action is taken.
 */
void free(void* ptr);

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
void* realloc(void* ptr, size_t new_size);

/**
 * @brief Allocates zero-initialized memory for an array.
 *
 * Equivalent to malloc(nmemb * size) followed by memset to zero.
 *
 * @param nmemb Number of array elements.
 * @param size Size of each element.
 * @return Pointer to zero-initialized memory, or NULL on failure.
 */
void* calloc(size_t nmemb, size_t size);

/**
 * @brief Terminate the current unit.
 *
 * This will stop the unit and remove it from its realm.
 *
 * @param code Exit code for the unit.
 * @return Does not return; halts the unit.
 */
_Noreturn void exit(uint64_t code);

/**
 * @brief Convert a string to an integer.
 *
 * Parses the string @p s as a decimal integer, optionally preceded by
 * whitespace and a sign character ('+' or '-'). Parsing stops at the
 * first character that is not a valid digit.
 *
 * @param s Null-terminated string to convert.
 * @return Converted integer value, or 0 if no valid conversion exists.
 *
 * @see atol()
 * @see strtol()
 */
int atoi(const char* s);

/**
 * @brief Convert a string to a long integer.
 *
 * Behaves like atoi() but returns a long.
 *
 * @param s Null-terminated string to convert.
 * @return Converted long value, or 0 if no valid conversion exists.
 */
long atol(const char* s);

double strtod(const char* str, char** endptr);
float strtof(const char* str, char** endptr);
long double strtold(const char* str, char** endptr);

static inline int abs(int x) {
    return x < 0 ? -x : x;
}
static inline long labs(long x) {
    return x < 0 ? -x : x;
}
static inline long long llabs(long long x) {
    return x < 0 ? -x : x;
}

_Noreturn void abort(void);

int system(const char* cmd);
char* tmpnam(char* buf);

#define L_tmpnam 32

#ifdef __cplusplus
}
#endif

#endif  // VESPERAOS_STDLIB_H
