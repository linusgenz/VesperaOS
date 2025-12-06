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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* Handle type definitions */
#define HANDLE_TYPE_CONSOLE 0x1000000000000000ULL
#define HANDLE_TYPE_FILE    0x2000000000000000ULL
#define HANDLE_TYPE_CHANNEL 0x3000000000000000ULL
#define HANDLE_TYPE_UNIT    0x4000000000000000ULL
#define HANDLE_TYPE_REALM   0x5000000000000000ULL
#define HANDLE_TYPE_DEVICE  0x6000000000000000ULL

/* Standard stream handles */
#define HANDLE_STDIN   (HANDLE_TYPE_CONSOLE | 0x0000000000000000ULL)
#define HANDLE_STDOUT  (HANDLE_TYPE_CONSOLE | 0x0000000000000001ULL)
#define HANDLE_STDERR  (HANDLE_TYPE_CONSOLE | 0x0000000000000002ULL)

typedef int64_t HANDLE_ID;
typedef HANDLE_ID FILE_HANDLE;
typedef HANDLE_ID CHANNEL_HANDLE;
typedef HANDLE_ID DIR_HANDLE;

#ifdef __cplusplus
extern "C" {



#endif

/**
 * @brief Write a single character to stdout.
 *
 * @param c Character to write (converted to unsigned char).
 * @return Character written on success, or negative error code on failure.
 *
 * @see puts()
 * @see getchar()
 */
int putchar(int c);

/**
 * @brief Write a null-terminated string to stdout.
 *
 * Writes the string @p s to stdout without appending a newline.
 *
 * @param s String to write (null-terminated).
 * @return Number of characters written, or @c -1 on error.
 *
 * @see putchar()
 */
int puts(const char* s);

/**
 * @brief Read a single character from stdin.
 *
 * @return Character read (as unsigned char cast to int), or @c -1 on error/EOF.
 *
 * @see putchar()
 */
int getchar(void);

/**
 * @brief Formatted output to stdout.
 *
 * Supports format specifiers: %s (string), %c (char), %d (int), %u (unsigned),
 * %x (hex), %p (pointer), %f (float), %lld (long long), %llu (unsigned long long).
 * Width and padding modifiers are supported (e.g., %02x, %8d).
 *
 * @param fmt Format string.
 * @param ... Variable arguments matching format specifiers.
 *
 * @see snprintf()
 */
void printf(const char* fmt, ...);

/**
* @brief Formatted output to a buffer with size limit.
*
* Writes at most @p size - 1 characters to @p buffer, always null-terminating.
* Supports format specifiers: %s, %c, %d, %x, %llu, %%.
*
* @param buffer Destination buffer.
* @param size Size of buffer (including null terminator).
* @param format Format string.
* @param ... Variable arguments matching format specifiers.
* @return Total number of characters that would have been written (excluding null),
*         or @c -1 on error.
*
* @see printf()
*/
size_t snprintf(char* buffer, size_t size, const char* format, ...);

/**
 * @brief Open a file.
 *
 * Opens the file at @p path with the specified flags.
 * Common flags: @c O_RDONLY, @c O_WRONLY, @c O_RDWR, @c O_CREAT.
 *
 * @param path Path to the file (null-terminated string).
 * @param flags Open flags (bitwise OR of O_* constants).
 * @return File handle on success, or negative error code on failure.
 *
 * @see fclose()
 * @see fread()
 * @see fwrite()
 */
FILE_HANDLE fopen(const char* path, int flags);

/**
 * @brief Close a file.
 *
 * Closes the file associated with @p handle and releases resources.
 *
 * @param handle File handle returned by fopen().
 * @return @c 0 on success, or negative error code on failure.
 *
 * @see fopen()
 */
int fclose(FILE_HANDLE handle);

/**
 * @brief Read data from a file.
 *
 * Reads up to @p count bytes from @p handle into @p buf.
 *
 * @param handle File handle returned by fopen().
 * @param buf Buffer to store read data.
 * @param count Maximum number of bytes to read.
 * @return Number of bytes read on success, or negative error code on failure.
 *
 * @see fwrite()
 * @see fopen()
 */
ssize_t fread(FILE_HANDLE handle, void* buf, size_t count);

/**
 * @brief Write data to a file.
 *
 * Writes @p count bytes from @p buf to @p handle.
 *
 * @param handle File handle returned by fopen().
 * @param buf Buffer containing data to write.
 * @param count Number of bytes to write.
 * @return Number of bytes written on success, or negative error code on failure.
 *
 * @see fread()
 * @see fopen()
 */
ssize_t fwrite(FILE_HANDLE handle, const void* buf, size_t count);


/**
 * @brief Reposition stream position indicator.
 *
 * Sets the file position indicator for @p stream to @p offset bytes
 * relative to @p whence.
 *
 * @param stream File handle returned by fopen().
 * @param offset Number of bytes to offset.
 * @param whence Seek mode (SEEK_SET, SEEK_CUR, or SEEK_END).
 * @return On success, returns the new absolute position from the beginning
 *         On error, returns negative errno:
 *           -EINVAL  : invalid handle, negative position, or invalid whence
 *           -EBADH   : handle not found or invalid resource
 *           -ESPIPE  : handle refers to a TTY or directory
 *           -EUNKNOWN: realm not found
 * @see ftell()
 * @see rewind()
 */
ssize_t fseek(FILE_HANDLE stream, long offset, int whence);

/**
 * @brief Get current position in stream.
 *
 * Returns the current file position indicator for @p stream.
 *
 * @param stream File handle returned by fopen().
 * @return On success, returns the new absolute position from the beginning
 *         On error, returns negative errno:
 *           -EINVAL  : invalid handle
 *           -EBADH   : handle not found or invalid resource
 *           -ESPIPE  : handle refers to a TTY or directory
 *           -EUNKNOWN: realm not found
 * @see fseek()
 * @see rewind()
 */
ssize_t ftell(FILE_HANDLE stream);

/**
 * @brief Reset file position to the beginning.
 *
 * Equivalent to fseek(stream, 0, SEEK_SET).
 *
 * @param stream File handle returned by fopen().
 * @return On success returns 0
 *        On error, returns negative errno:
 *           -EINVAL  : invalid handle
 *           -EBADH   : handle not found or invalid resource
 *           -ESPIPE  : handle refers to a TTY or directory
 *           -EUNKNOWN: realm not found
 * @see fseek()
 * @see ftell()
 */
int rewind(FILE_HANDLE stream);

/**
 * @brief Open a directory.
 *
 * Opens the directory at @p path for reading directory entries.
 *
 * @param path Path to the directory (null-terminated string).
 * @return Directory handle on success, or negative error code on failure.
 *
 * @see close()
 */
DIR_HANDLE opendir(const char* path);

/**
 * @brief Close a directory.
 *
 * Closes the directory handle returned by opendir().
 *
 * @param handle Directory handle returned by opendir().
 * @return @c 0 on success, or negative error code on failure.
 *
 * @see opendir()
 * @see close()
 */
int closedir(DIR_HANDLE handle);

/**
* @brief Open a resource (generic handle).
*
* Lower-level version of fopen() that returns a generic handle.
*
* @param path Path to the resource (null-terminated string).
* @param flags Open flags (bitwise OR of O_* constants).
* @return Handle on success, or negative error code on failure.
*
* @see close()
* @see fopen()
*/
HANDLE_ID open(const char* path, int flags);

/**
 * @brief Close a generic handle.
 *
 * Lower-level version of fclose() for generic handles.
 *
 * @param handle Handle returned by open().
 * @return @c 0 on success, or negative error code on failure.
 *
 * @see open()
 * @see fclose()
 */
int close(HANDLE_ID handle);


#ifdef __cplusplus
}
#endif

#endif //VESPERAOS_STDIO_H
