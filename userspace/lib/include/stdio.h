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
#include <stdlib.h>

#include "dirent.h"

/* Default buffer size.  */
#define BUFSIZ 8192

/* The value returned by fgetc and similar functions to indicate the
   end of the file.  */
#define EOF (-1)

#define getc(f) fgetc(f)

typedef uint64_t HANDLE;
typedef HANDLE FILE_HANDLE;
typedef HANDLE CHANNEL_HANDLE;
typedef HANDLE DIR_HANDLE;

typedef struct FILE {
    FILE_HANDLE handle;
    int error;
    int eof;
    int unget_char;
    uint8_t* buffer;
    size_t buf_size;
    size_t buf_pos;
} FILE;

extern FILE* stdin;  /* Standard input stream.  */
extern FILE* stdout; /* Standard output stream.  */
extern FILE* stderr; /* Standard error output stream.  */
#define stdin stdin
#define stdout stdout
#define stderr stderr

#define SEEK_SET 0 /**< Seek relative to start of file */
#define SEEK_CUR 1 /**< Seek relative to current position */
#define SEEK_END 2 /**< Seek relative to end of file */

#ifdef __cplusplus
extern "C" {

#endif

/* Read formatted input from S.  */
int sscanf(const char* str, const char* format, ...);

int fscanf(FILE* f, const char* format, ...);

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

int fgetc(FILE* f);

int ungetc(int c, FILE* f);

char* fgets(char* buf, int n, FILE* f);

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

int vfprintf(FILE* f, const char* fmt, va_list args);
int fprintf(FILE* f, const char* fmt, ...);
int vprintf(const char* fmt, va_list args);

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
 * @todo add docs
 */
FILE* fopen(const char* path, const char* mode);

FILE* freopen(const char* path, const char* mode, FILE* f);

/**
 * @todo add docs
 */
int fclose(FILE* f);

/**
 * @todo add docs
 */
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* f);

/**
 * @todo add docs
 */
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* f);

/**
 * @todo add docs
 */
int fputs(const char* s, FILE* f);

/**
 * @todo add docs
 */
int fputc(int c, FILE* f);

/**
 * @brief Test the end-of-file indicator.
 *
 * Returns non-zero if the EOF indicator is set on @p f, i.e. the last
 * read operation reached the end of the file.  The indicator is cleared
 * by a successful seek (fseek / rewind).
 *
 * @param f File pointer returned by fopen().
 * @return Non-zero if EOF has been reached, @c 0 otherwise.
 *
 * @see ferror()
 * @see fseek()
 */
int feof(FILE* f);

int ferror(FILE* f);
int fflush(FILE* f);

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
int fseek(FILE* stream, long offset, int whence);

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
ssize_t ftell(FILE* stream);

/**
 * @brief Reset file position to the beginning.
 *
 * Equivalent to fseek(stream, 0, SEEK_SET).
 *
 * @param stream File handle returned by fopen().
 * @see fseek()
 * @see ftell()
 */
void rewind(FILE* stream);

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
 * @brief Read a directory entry.
 *
 * Reads the next directory entry from @p handle into @p entry.
 * Returns @c 0 when no more entries are available.
 *
 * @param handle Directory handle returned by opendir().
 * @param entry Pointer to dirent_t structure to fill.
 * @return Positive value on success, @c 0 at end of directory,
 *         or negative error code on failure.
 *
 * @see opendir()
 * @see closedir()
 */
ssize_t readdir(DIR_HANDLE handle, dirent_t* entry);

/**
 * @brief Open a resource (generic handle).
 *
 * Lower-level version of fopen() that returns a generic handle.
 *
 * @param path Path to the resource (null-terminated string).
 * @param flags Open flags (bitwise OR of O_* constants, see @ref fflags.h).
 * @return Handle on success, or negative error code on failure.
 *
 * @see close()
 * @see fopen()
 */
HANDLE open(const char* path, int flags);

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
int close(HANDLE handle);

/**
 * @brief Read data from a generic handle.
 *
 * Lower-level version of fread() for generic handles.
 *
 * @param handle File handle returned by open().
 * @param buf Buffer to store read data.
 * @param count Maximum number of bytes to read.
 * @return Number of bytes read on success, or negative error code on failure.
 *
 * @see write()
 * @see open()
 */
ssize_t read(HANDLE handle, void* buf, size_t count);

/**
 * @brief Write data to a generic handle.
 *
 * Lower-level version of fwrite() for generic handles.
 *
 * @param handle File handle returned by open().
 * @param buf Buffer containing data to write.
 * @param count Number of bytes to write.
 * @return Number of bytes written on success, or negative error code on failure.
 *
 * @see read()
 * @see open()
 */
ssize_t write(FILE_HANDLE handle, const void* buf, size_t count);

/**
 * @brief Create a new file.
 *
 * Creates a new empty file at @p path.
 *
 * @param path Path where the file should be created.
 * @return @c 0 on success, or negative error code on failure.
 *
 * @see fopen()
 * @see mkdir()
 */
int creat(const char* path);

/**
 * @brief Create a new directory.
 *
 * Creates a new directory at @p path.
 *
 * @param path Path where the directory should be created.
 * @return @c 0 on success, or negative error code on failure.
 *
 * @see opendir()
 * @see creat()
 */
int mkdir(const char* path);

/**
 * @brief Remove a file.
 *
 * Removes (deletes) the file at @p path. The file must not be open.
 *
 * @param path Path to the file to remove.
 * @return @c 0 on success, or negative error code on failure.
 *
 * @see creat()
 * @see rmdir()
 */
int unlink(const char* path);

/**
 * @brief Remove a directory.
 *
 * Removes (deletes) the directory at @p path. The directory must be empty.
 *
 * @param path Path to the directory to remove.
 * @return @c 0 on success, or negative error code on failure.
 *
 * @see mkdir()
 * @see unlink()
 */
int rmdir(const char* path);

/**
 * @brief Create a new file or directory.
 *
 * Creates a new file or directory at @p path. The type is determined by flags.
 *
 * @param path Path where the file/directory should be created.
 * @param flags Creation flags .
 * @return @c 0 on success, or negative error code on failure.
 *
 * @see fopen()
 * @see opendir()
 */
int create(const char* path, int flags);

/**
 * @brief Change the current working directory.
 *
 * Resolves the given path (relative or absolute) and updates the
 * calling realm's working directory if the target exists and is
 * a directory.
 *
 * @param path Null-terminated path string (absolute or relative).
 * @return 0 on success, or negative errno:
 *           -EINVAL  : path is null or empty
 *           -ENOENT  : path does not exist
 *           -ENOTDIR : path exists but is not a directory
 *
 * @see getcwd()
 */
int chdir(const char* path);

/**
 * @brief Get the current working directory.
 *
 * Copies the absolute path of the calling realm's current working
 * directory into @p buf. The buffer must be large enough to hold
 * the full path including the null terminator.
 *
 * @param buf  Buffer to store the path string.
 * @param size Size of @p buf in bytes.
 * @return @p buf on success, or @c NULL on failure (errno set):
 *           EINVAL : buf is null or size is zero
 *           ERANGE : buffer too small to hold the current path
 *
 * @see chdir()
 */
int getcwd(char* buf, size_t size);

FILE* tmpfile(void);

void clearerr(FILE* f);

#define _IOFBF 0  // fully buffered
#define _IOLBF 1  // line buffered
#define _IONBF 2  // unbuffered

int setvbuf(FILE* f, char* buf, int mode, size_t size);

int remove(const char* path);
int rename(const char* oldpath, const char* newpath);

#ifdef __cplusplus
}
#endif

#endif  // VESPERAOS_STDIO_H
