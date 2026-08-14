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

#define _IOFBF 0  // fully buffered
#define _IOLBF 1  // line buffered
#define _IONBF 2  // unbuffered

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read formatted input from a null-terminated string.
 *
 * @param str Input string to parse.
 * @param format Format string specifying format specifiers.
 * @param ... Pointer arguments where parsed values are stored.
 * @return Number of input items successfully matched and assigned,
 *         or @c -1 on failure.
 *
 * @see fscanf()
 * @see printf()
 */
int sscanf(const char* str, const char* format, ...);

/**
 * @brief Read formatted input from a file stream.
 *
 * @param f Source file stream.
 * @param format Format string specifying format specifiers.
 * @param ... Pointer arguments where parsed values are stored.
 * @return Number of input items successfully matched and assigned,
 *         or @c -1 on failure/EOF.
 *
 * @see sscanf()
 * @see fgetc()
 */
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
 * Writes the string @p s followed by a newline character to stdout.
 *
 * @param s String to write (null-terminated).
 * @return @c 0 on success, or @c -1 on error (e.g., @p s is NULL).
 *
 * @see putchar()
 * @see fputs()
 */
int puts(const char* s);

/**
 * @brief Read a single character from stdin.
 *
 * @return Character read (as unsigned char cast to int), or @c -1 on error/EOF.
 *
 * @see putchar()
 * @see fgetc()
 */
int getchar(void);

/**
 * @brief Read a single character from a file stream.
 *
 * @param f Source file stream.
 * @return Character read (as unsigned char cast to int), or @c EOF (-1) on error/EOF.
 *
 * @see fputc()
 * @see fgets()
 * @see getchar()
 */
int fgetc(FILE* f);

/**
 * @brief Push a character back onto a file stream.
 *
 * Pushes @p c back to @p f so that it will be returned by the next read operation.
 * Clears the EOF indicator for the stream.
 *
 * @param c Character to push back.
 * @param f File stream pointer.
 * @return The character pushed back as @c unsigned @c char cast to @c int,
 *         or @c -1 on error.
 *
 * @see fgetc()
 */
int ungetc(int c, FILE* f);

/**
 * @brief Read a line or string from a file stream.
 *
 * Reads at most @p n - 1 characters from @p f and stores them in @p buf.
 * Reading stops after an EOF or newline character is read.
 *
 * @param buf Destination buffer.
 * @param n Maximum number of characters to read (including null terminator).
 * @param f Source file stream.
 * @return @p buf on success, or @c NULL on error or EOF before any character was read.
 *
 * @see fgetc()
 * @see fputs()
 */
char* fgets(char* buf, int n, FILE* f);

/**
 * @brief Formatted output to stdout.
 *
 * Supports format specifiers: %s, %c, %d, %i, %u, %x, %X, %o, %p, %f, %e, %g.
 * Width, padding, and length modifiers (e.g. %llu, %02x) are supported.
 *
 * @param fmt Format string.
 * @param ... Variable arguments matching format specifiers.
 * @return Total number of characters written.
 *
 * @see vprintf()
 * @see snprintf()
 */
int printf(const char* fmt, ...);

/**
 * @brief Write formatted output to a file stream using a va_list.
 *
 * @param f Destination file stream.
 * @param fmt Format string.
 * @param args Variable argument list.
 * @return Number of characters written on success, or @c -1 on error.
 *
 * @see fprintf()
 * @see vprintf()
 */
int vfprintf(FILE* f, const char* fmt, va_list args);

/**
 * @brief Write formatted output to a file stream.
 *
 * @param f Destination file stream.
 * @param fmt Format string.
 * @param ... Variable arguments matching format specifiers.
 * @return Number of characters written on success, or @c -1 on error.
 *
 * @see vfprintf()
 * @see printf()
 */
int fprintf(FILE* f, const char* fmt, ...);

/**
 * @brief Formatted output to stdout using a va_list.
 *
 * @param fmt Format string.
 * @param args Variable argument list.
 * @return Total number of characters written.
 *
 * @see printf()
 * @see vfprintf()
 */
int vprintf(const char* fmt, va_list args);

/**
 * @brief Formatted output to a buffer with size limit.
 *
 * Writes at most @p size - 1 characters to @p buffer, always null-terminating.
 *
 * @param buffer Destination buffer.
 * @param size Size of buffer (including null terminator).
 * @param format Format string.
 * @param ... Variable arguments matching format specifiers.
 * @return Total number of characters written/formatted, or @c -1 on error.
 *
 * @see vsnprintf()
 * @see printf()
 */
int snprintf(char* buffer, size_t size, const char* format, ...);

/**
 * @brief Formatted output to a buffer with size limit using a va_list.
 *
 * @param buffer Destination buffer.
 * @param size Size of buffer (including null terminator).
 * @param fmt Format string.
 * @param args Variable argument list.
 * @return Total number of characters written/formatted, or @c -1 on error.
 *
 * @see snprintf()
 */
int vsnprintf(char* buffer, size_t size, const char* fmt, va_list args);

/**
 * @brief Open a file stream.
 *
 * Opens the file specified by @p path with the given @p mode ("r", "w", "a", "r+", etc.).
 *
 * @param path Path to the file.
 * @param mode Mode string.
 * @return Pointer to allocated FILE structure on success, or @c NULL on error (errno set).
 *
 * @see fclose()
 * @see freopen()
 * @see open()
 */
FILE* fopen(const char* path, const char* mode);

/**
 * @brief Reopen an existing file stream with a new path and mode.
 *
 * Flushes and closes the existing file handle in @p f and opens the file at @p path.
 *
 * @param path Path to the new file.
 * @param mode Mode string.
 * @param f Existing FILE stream to reuse.
 * @return Pointer to @p f on success, or @c NULL on error.
 *
 * @see fopen()
 * @see fclose()
 */
FILE* freopen(const char* path, const char* mode, FILE* f);

/**
 * @brief Close a file stream.
 *
 * Flushes buffered data, closes the underlying handle, and frees stream resources.
 *
 * @param f File stream to close.
 * @return @c 0 on success, or @c -1 on error (errno set).
 *
 * @see fopen()
 * @see close()
 */
int fclose(FILE* f);

/**
 * @brief Read data from a file stream.
 *
 * Reads up to @p nmemb elements, each of size @p size, from stream @p f into @p ptr.
 *
 * @param ptr Memory buffer to store read data.
 * @param size Size of each element in bytes.
 * @param nmemb Number of elements to read.
 * @param f Source file stream.
 * @return Number of elements successfully read.
 *
 * @see fwrite()
 * @see read()
 */
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* f);

/**
 * @brief Write data to a file stream.
 *
 * Writes up to @p nmemb elements, each of size @p size, from @p ptr to stream @p f.
 *
 * @param ptr Memory buffer containing data to write.
 * @param size Size of each element in bytes.
 * @param nmemb Number of elements to write.
 * @param f Destination file stream.
 * @return Number of elements successfully written.
 *
 * @see fread()
 * @see write()
 */
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* f);

/**
 * @brief Write a string to a file stream.
 *
 * Writes the null-terminated string @p s to stream @p f without appending a newline.
 *
 * @param s Null-terminated string to write.
 * @param f Destination file stream.
 * @return @c 0 on success, or @c EOF (-1) on error.
 *
 * @see puts()
 * @see fputc()
 */
int fputs(const char* s, FILE* f);

/**
 * @brief Write a character to a file stream.
 *
 * Writes character @p c (converted to unsigned char) to stream @p f.
 *
 * @param c Character to write.
 * @param f Destination file stream.
 * @return Written character on success, or @c EOF (-1) on error.
 *
 * @see fgetc()
 * @see putchar()
 */
int fputc(int c, FILE* f);

/**
 * @brief Test the end-of-file indicator.
 *
 * Returns non-zero if the EOF indicator is set on @p f.
 *
 * @param f File pointer returned by fopen().
 * @return Non-zero if EOF has been reached, @c 0 otherwise.
 *
 * @see ferror()
 * @see fseek()
 */
int feof(FILE* f);

/**
 * @brief Test the error indicator for a file stream.
 *
 * @param f File stream pointer.
 * @return Non-zero if error flag is set, @c 0 otherwise.
 *
 * @see feof()
 * @see clearerr()
 */
int ferror(FILE* f);

/**
 * @brief Flush buffered output on a file stream.
 *
 * Flushes buffered output for stream @p f. If @p f is NULL or stdout,
 * flushes the stdout buffer.
 *
 * @param f File stream pointer, or NULL for stdout.
 * @return @c 0 on success, or @c EOF (-1) on error.
 *
 * @see fclose()
 */
int fflush(FILE* f);

/**
 * @brief Reposition stream position indicator.
 *
 * Sets the file position indicator for @p stream to @p offset bytes relative to @p whence.
 *
 * @param stream File stream returned by fopen().
 * @param offset Number of bytes to offset.
 * @param whence Seek mode (SEEK_SET, SEEK_CUR, or SEEK_END).
 * @return @c 0 on success, or @c -1 on error (errno set).
 *
 * @see ftell()
 * @see rewind()
 * @see lseek()
 */
int fseek(FILE* stream, long offset, int whence);

/**
 * @brief Reposition file offset for a generic handle.
 *
 * Lower-level version of fseek() operating on raw handles.
 *
 * @param handle File handle returned by open().
 * @param offset Offset in bytes.
 * @param whence Seek mode (SEEK_SET, SEEK_CUR, or SEEK_END).
 * @return New absolute position from beginning of file on success, or @c -1 on error (errno set).
 *
 * @see fseek()
 */
int64_t lseek(HANDLE handle, int64_t offset, int whence);

/**
 * @brief Get current position in stream.
 *
 * Returns the current file position indicator for @p stream in bytes from the start.
 *
 * @param stream File stream returned by fopen().
 * @return Current absolute position on success, or negative value / @c -1 on error.
 *
 * @see fseek()
 * @see rewind()
 */
ssize_t ftell(FILE* stream);

/**
 * @brief Reset file position to the beginning.
 *
 * Resets position indicator, ungetc buffer, EOF flag, and error flag.
 *
 * @param stream File stream returned by fopen().
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
 * @return Directory handle on success, or @c (DIR_HANDLE)-1 on failure (errno set).
 *
 * @see closedir()
 * @see readdir()
 */
DIR_HANDLE opendir(const char* path);

/**
 * @brief Close a directory handle.
 *
 * Closes the directory handle returned by opendir().
 *
 * @param handle Directory handle returned by opendir().
 * @return @c 0 on success, or @c -1 on error (errno set).
 *
 * @see opendir()
 */
int closedir(DIR_HANDLE handle);

/**
 * @brief Read a directory entry.
 *
 * Reads the next directory entry from @p handle into @p entry.
 *
 * @param handle Directory handle returned by opendir().
 * @param entry Pointer to dirent_t structure to fill.
 * @return Number of bytes read on success, @c 0 at end of directory,
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
 * @return Handle on success, or @c (HANDLE)-1 on failure (errno set).
 *
 * @see close()
 * @see fopen()
 */
HANDLE vopen(const char* path, int flags);

/**
 * @brief Close a generic handle.
 *
 * Lower-level version of fclose() for generic handles.
 *
 * @param handle Handle returned by open().
 * @return @c 0 on success, or @c -1 on failure (errno set).
 *
 * @see open()
 * @see fclose()
 */
int vclose(HANDLE handle);

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
ssize_t vread(HANDLE handle, void* buf, size_t count);

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
ssize_t vwrite(HANDLE handle, const void* buf, size_t count);

/**
 * @brief Create a new empty file.
 *
 * Creates a new file at @p path.
 *
 * @param path Path where the file should be created.
 * @return @c 0 on success, or @c -1 on failure (errno set).
 *
 * @see fopen()
 * @see mkdir()
 * @see create()
 */
int creat(const char* path);

/**
 * @brief Create a new directory.
 *
 * Creates a new directory at @p path.
 *
 * @param path Path where the directory should be created.
 * @return @c 0 on success, or @c -1 on failure (errno set).
 *
 * @see opendir()
 * @see creat()
 * @see rmdir()
 */
int mkdir(const char* path);

/**
 * @brief Remove a file.
 *
 * Removes (deletes) the file at @p path.
 *
 * @param path Path to the file to remove.
 * @return @c 0 on success, or @c -1 on failure (errno set).
 *
 * @see creat()
 * @see rmdir()
 * @see remove()
 */
int unlink(const char* path);

/**
 * @brief Remove a directory.
 *
 * Removes (deletes) the empty directory at @p path.
 *
 * @param path Path to the directory to remove.
 * @return @c 0 on success, or @c -1 on failure (errno set).
 *
 * @see mkdir()
 * @see unlink()
 * @see remove()
 */
int rmdir(const char* path);

/**
 * @brief Create a new file or directory.
 *
 * Creates a new resource at @p path based on @p type (e.g. C_DIR).
 *
 * @param path Path where the file/directory should be created.
 * @param type Resource type flag.
 * @return @c 0 on success, or @c -1 on failure (errno set).
 *
 * @see creat()
 * @see mkdir()
 */
int create(const char* path, int type);

/**
 * @brief Change the current working directory.
 *
 * Updates the calling realm's current working directory to @p path.
 *
 * @param path Null-terminated path string.
 * @return @c 0 on success, or @c -1 on failure (errno set to EINVAL, ENOENT, ENOTDIR, etc.).
 *
 * @see getcwd()
 * @see chroot()
 */
int chdir(const char* path);

/**
 * @brief Change root directory.
 *
 * Changes the root directory of the calling realm to @p path.
 *
 * @param path Path to the new root directory.
 * @return @c 0 on success, or @c -1 on failure (errno set).
 *
 * @see chdir()
 */
int chroot(const char* path);

/**
 * @brief Get the current working directory.
 *
 * Copies the absolute path of the calling realm's current working
 * directory into @p buf.
 *
 * @param buf Buffer to store the path string.
 * @param size Size of @p buf in bytes.
 * @return @p buf on success, or @c NULL on failure (errno set to EINVAL, ERANGE, etc.).
 *
 * @see chdir()
 */
char* getcwd(char* buf, size_t size);

/**
 * @brief Create a temporary binary file.
 *
 * Creates a unique temporary file in "/tmp" opened in update mode ("w+").
 * The file is unlinked immediately so it will be automatically removed when closed.
 *
 * @return FILE stream pointer on success, or @c NULL on error.
 */
FILE* tmpfile(void);

/**
 * @brief Clear error and EOF indicators for a stream.
 *
 * Resets the error flag, EOF flag, and ungetc buffer for @p f.
 *
 * @param f Source file stream.
 *
 * @see ferror()
 * @see feof()
 */
void clearerr(FILE* f);

/**
 * @brief Set stream buffering mode.
 *
 * @param f File stream pointer.
 * @param buf User-provided buffer (optional).
 * @param mode Buffering mode (_IOFBF, _IOLBF, _IONBF).
 * @param size Size of buffer.
 * @return @c 0 on success, or @c -1 if @p f is NULL.
 */
int setvbuf(FILE* f, char* buf, int mode, size_t size);

/**
 * @brief Remove a file or directory.
 *
 * Attempts to remove the file via unlink, or if that fails, removes the directory via rmdir.
 *
 * @param path Path to file or directory.
 * @return @c 0 on success, or @c -1 on error (errno set).
 *
 * @see unlink()
 * @see rmdir()
 */
int remove(const char* path);

/**
 * @brief Rename or move a file or directory.
 *
 * @param oldpath Current path.
 * @param newpath New path.
 * @return @c 0 on success, or @c -1 on error (errno set).
 *
 * @see remove()
 */
int rename(const char* oldpath, const char* newpath);

#ifdef __cplusplus
}
#endif

#endif  // VESPERAOS_STDIO_H