// stdio.h
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

#ifndef VESPERAOS_STDIO_H
#define VESPERAOS_STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "unistdio.h"

#define HANDLE_TYPE_CONSOLE 0x1000000000000000ULL
#define HANDLE_TYPE_FILE    0x2000000000000000ULL
#define HANDLE_TYPE_CHANNEL 0x3000000000000000ULL
#define HANDLE_TYPE_UNIT    0x4000000000000000ULL
#define HANDLE_TYPE_REALM   0x5000000000000000ULL
#define HANDLE_TYPE_DEVICE  0x6000000000000000ULL

#define HANDLE_STDIN   (HANDLE_TYPE_CONSOLE | 0x0000000000000000ULL)
#define HANDLE_STDOUT  (HANDLE_TYPE_CONSOLE | 0x0000000000000001ULL)
#define HANDLE_STDERR  (HANDLE_TYPE_CONSOLE | 0x0000000000000002ULL)

typedef int64_t FILE_HANDLE;
typedef long long ssize_t;

/**
 * @brief Write a single character to stdout.
 *
 * @param c Character to write.
 * @return Character written, or negative on error.
 */
int putchar(int c);

/**
 * @brief Write a null-terminated string to stdout.
 *
 * @param s String to write.
 * @return Number of characters written, or negative on error.
 */
int puts(const char *s);

/**
 * @brief Read a single character from stdin.
 *
 * @return Character read, or negative on error.
 */
int getchar(void);

/**
 * @brief Formatted output to stdout.
 *
 * Supported formats: %s, %c, %d, %x
 *
 * @param fmt Format string.
 * @param ... Arguments.
 * @return Number of characters written.
 */
void printf(const char *fmt, ...);

/**
 * @brief Opens a file.
 * @param path Path to the file
 * @param flags Flags (O_RDONLY, O_WRONLY, O_RDWR, O_CREAT)
 * @return Handle or negative error code
 */
FILE_HANDLE fopen(const char* path, int flags);

/**
 * @brief Closes a file.
 * @param handle Open handle.
 * @return 0 on success, negative error code on failure.
 */
int fclose(FILE_HANDLE handle);

/**
 * @brief Reads data from a handle
 * @param handle Handle of the file
 * @param buf Buffer to fill
 * @param count Number of bytes
 * @return Number of bytes read or negative error code
 */
ssize_t fread(FILE_HANDLE handle, void* buf, size_t count);

/**
 * @brief Writes data to a handle
 * @param handle Handle of the file
 * @param buf Buffer with data
 * @param count Number of bytes
 * @return Number of bytes written or negative error code
 */
ssize_t fwrite(FILE_HANDLE handle, const void* buf, size_t count);


#endif //VESPERAOS_STDIO_H