// stdio.c
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

#include <fflags.h>
#include <sysstd.h>
#include <stdio.h>
#include <string.h>
#include <internal.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>

static const char* skip_whitespace(const char* str)
{
    while (*str && isspace((unsigned char)*str))
    {
        str++;
    }
    return str;
}

// Hilfsfunktion: long int parsen
static bool parse_long(const char** str, long* result)
{
    const char* s = *str;
    long val = 0;
    int sign = 1;
    bool has_digits = false;

    // Vorzeichen behandeln
    if (*s == '-')
    {
        sign = -1;
        s++;
    }
    else if (*s == '+')
    {
        s++;
    }

    // Ziffern parsen
    while (*s && isdigit((unsigned char)*s))
    {
        int digit = *s - '0';

        // Overflow-Prüfung
        if (sign == 1)
        {
            if (val > (LONG_MAX - digit) / 10)
            {
                return false; // Overflow
            }
        }
        else
        {
            if (val > (LONG_MAX - digit) / 10)
            {
                return false; // Overflow (bei negativen Zahlen)
            }
        }

        val = val * 10 + digit;
        s++;
        has_digits = true;
    }

    if (!has_digits)
    {
        return false;
    }

    *result = sign * val;
    *str = s;
    return true;
}

int sscanf(const char* str, const char* format, ...)
{
    if (!str || !format)
    {
        return -1;
    }

    va_list args;
    va_start(args, format);

    int matches = 0;
    const char* s = str;
    const char* f = format;

    while (*f && *s)
    {
        // Whitespace im Format überspringt beliebig viel Whitespace im String
        if (isspace((unsigned char)*f))
        {
            s = skip_whitespace(s);
            f++;
            continue;
        }

        // Konvertierungsspezifikation
        if (*f == '%')
        {
            f++;

            if (*f == '%')
            {
                // Literales '%'
                if (*s != '%')
                {
                    break;
                }
                s++;
                f++;
                continue;
            }

            // Whitespace vor Konvertierung überspringen (außer bei %c)
            if (*f != 'c')
            {
                s = skip_whitespace(s);
            }

            if (*f == 'l')
            {
                f++;
                if (*f == 'd')
                {
                    // %ld - long int
                    long* ptr = va_arg(args, long*);
                    if (!ptr)
                    {
                        va_end(args);
                        return -1;
                    }

                    long val;
                    if (parse_long(&s, &val))
                    {
                        *ptr = val;
                        matches++;
                    }
                    else
                    {
                        break; // Parsing fehlgeschlagen
                    }
                    f++;
                }
            }
            else if (*f == 'c')
            {
                // %c - einzelnes Zeichen (überspringt KEIN Whitespace)
                char* ptr = va_arg(args, char*);
                if (!ptr)
                {
                    va_end(args);
                    return -1;
                }

                if (*s)
                {
                    *ptr = *s;
                    s++;
                    matches++;
                }
                else
                {
                    break; // Keine Zeichen mehr verfügbar
                }
                f++;
            }
            else
            {
                // Unbekannter Format-Spezifizierer
                break;
            }
        }
        else
        {
            // Literales Zeichen im Format muss übereinstimmen
            if (*f != *s)
            {
                break;
            }
            f++;
            s++;
        }
    }

    va_end(args);
    return matches;
}

int putchar(int c)
{
    char ch = (char)c;
    return (int)sys_write(stdout, (uint64_t)&ch, 1, 0, 0, 0);
}

int puts(const char* s)
{
    if (!s) return -1;
    size_t len = 0;
    while (s[len]) len++;
    int ret = (int)sys_write(stdout, (uint64_t)s, len, 0, 0, 0);
    return ret;
}

int getchar(void)
{
    char ch;
    int ret = (int)sys_read(stdin, (uint64_t)&ch, 1, 0, 0, 0);
    if (ret <= 0) return -1;
    return (int)ch;
}

#define PRINTF_BUFFER_SIZE 1024

static char printf_buffer[PRINTF_BUFFER_SIZE];
static size_t printf_pos = 0;

static void flush_printf_buffer()
{
    if (printf_pos > 0)
    {
        sys_write(stdout, (uint64_t)printf_buffer, printf_pos, 0, 0, 0);
        printf_pos = 0;
    }
}

static void buffer_putc(char c)
{
    printf_buffer[printf_pos++] = c;
    if (printf_pos == PRINTF_BUFFER_SIZE)
        flush_printf_buffer();
}

static void buffer_puts(const char* s)
{
    while (*s)
    {
        buffer_putc(*s++);
    }
}

void printf(const char* fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    char chr;
    while ((chr = *fmt++) != 0)
    {
        if (chr == '%')
        {
            // Flags & Width
            bool long_long = false;
            bool long_flag = false;
            char pad_char = ' ';
            int min_width = 0;

            // Padding: z. B. %02x → '0' erkannt
            if (*fmt == '0')
            {
                pad_char = '0';
                fmt++;
            }

            // Breite (z. B. 2, 4, 8, etc.)
            while (*fmt >= '0' && *fmt <= '9')
            {
                min_width = min_width * 10 + (*fmt - '0');
                fmt++;
            }

            // Länge: l / ll
            if (*fmt == 'l')
            {
                fmt++;
                if (*fmt == 'l')
                {
                    long_long = true;
                    fmt++;
                }
                else
                {
                    long_flag = true;
                }
            }

            char specifier = *fmt++;
            char buffer[64];

            switch (specifier)
            {
            case 's':
                {
                    const char* str = __builtin_va_arg(args, const char*);
                    buffer_puts(str ? str : "<null>");
                    break;
                }
            case 'u':
            case 'x':
                {
                    uint64_t val = (long_long || long_flag)
                                       ? __builtin_va_arg(args, uint64_t)
                                       : __builtin_va_arg(args, uint32_t);
                    int base = (specifier == 'x') ? 16 : 10;
                    uint_to_str(val, buffer, base, false);

                    // Padding manuell
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        buffer_putc(pad_char);

                    buffer_puts(buffer);
                    break;
                }
            case 'c':
                {
                    int val = __builtin_va_arg(args, int);
                    buffer_putc((char)val);
                    break;
                }
            case 'd':
                {
                    int64_t val = (long_long || long_flag)
                                      ? __builtin_va_arg(args, int64_t)
                                      : __builtin_va_arg(args, int32_t);
                    if (val < 0)
                    {
                        buffer_putc('-');
                        val = -val;
                    }
                    uint_to_str((uint64_t)val, buffer, 10, false);
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        buffer_putc(pad_char);
                    buffer_puts(buffer);
                    break;
                }
            case 'f':
                {
                    // Neuer Float-Support
                    double val = __builtin_va_arg(args, double);
                    float_to_str((float)val, buffer, 6); // 6 Nachkommastellen Standard
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        buffer_putc(pad_char);
                    buffer_puts(buffer);
                    break;
                }
            case 'p':
                {
                    uintptr_t val = __builtin_va_arg(args, uintptr_t);
                    puts("0x");
                    uint_to_str(val, buffer, 16, false);
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        buffer_putc('0');
                    puts(buffer);
                    break;
                }
            case '%':
                buffer_putc('%');
                break;
            default:
                buffer_putc('%');
                buffer_putc(specifier);
                break;
            }
        }
        else
        {
            buffer_putc(chr);
        }
    }
    __builtin_va_end(args);
    flush_printf_buffer();
}


size_t snprintf(char* buffer, size_t size, const char* format, ...)
{
    if (!buffer || !format || size == 0)
    {
        return -1;
    }

    __builtin_va_list args;
    __builtin_va_start(args, format);

    size_t buf_pos = 0;
    size_t written = 0;

    for (int i = 0; format[i] != '\0'; i++)
    {
        if (format[i] == '%' && format[i + 1] != '\0')
        {
            i++; // Skip '%'

            // Parse flags (zero-padding)
            bool zero_pad = false;
            if (format[i] == '0')
            {
                zero_pad = true;
                i++;
            }

            // Parse width
            int width = 0;
            while (format[i] >= '0' && format[i] <= '9')
            {
                width = width * 10 + (format[i] - '0');
                i++;
            }

            // Parse length modifier
            bool is_long_long = false;
            if (format[i] == 'l')
            {
                i++;
                if (format[i] == 'l')
                {
                    is_long_long = true;
                    i++;
                }
            }

            switch (format[i])
            {
            case 'd':
            case 'i':
                {
                    int val = __builtin_va_arg(args, int);
                    char temp[32];
                    size_t len = uint_to_str(val, temp, 10, false);

                    // Apply padding
                    int pad_count = width > len ? width - len : 0;
                    char pad_char = zero_pad ? '0' : ' ';

                    // Handle negative numbers with zero padding
                    if (zero_pad && temp[0] == '-')
                    {
                        if (buf_pos < size - 1)
                        {
                            buffer[buf_pos++] = '-';
                        }
                        written++;

                        for (int p = 0; p < pad_count && buf_pos < size - 1; p++)
                        {
                            buffer[buf_pos++] = '0';
                        }
                        written += pad_count;

                        for (int j = 1; j < len && buf_pos < size - 1; j++)
                        {
                            buffer[buf_pos++] = temp[j];
                        }
                        written += len - 1;
                    }
                    else
                    {
                        for (int p = 0; p < pad_count && buf_pos < size - 1; p++)
                        {
                            buffer[buf_pos++] = pad_char;
                        }
                        written += pad_count;

                        for (int j = 0; j < len && buf_pos < size - 1; j++)
                        {
                            buffer[buf_pos++] = temp[j];
                        }
                        written += len;
                    }
                    break;
                }

            case 'u':
                {
                    unsigned int val = __builtin_va_arg(args, unsigned int);
                    char temp[32];
                    size_t len = uint_to_str(val, temp, 10, false);

                    // Apply padding
                    int pad_count = width > len ? width - len : 0;
                    char pad_char = zero_pad ? '0' : ' ';

                    for (int p = 0; p < pad_count && buf_pos < size - 1; p++)
                    {
                        buffer[buf_pos++] = pad_char;
                    }
                    written += pad_count;

                    for (int j = 0; j < len && buf_pos < size - 1; j++)
                    {
                        buffer[buf_pos++] = temp[j];
                    }
                    written += len;
                    break;
                }

            case 'x':
            case 'X':
                {
                    unsigned int val = __builtin_va_arg(args, unsigned int);
                    char temp[32];
                    const size_t len = uint_to_str(val, temp, 16, false);

                    int pad_count = width > len ? width - len : 0;
                    char pad_char = zero_pad ? '0' : ' ';

                    for (int p = 0; p < pad_count && buf_pos < size - 1; p++)
                    {
                        buffer[buf_pos++] = pad_char;
                    }
                    written += pad_count;

                    for (int j = 0; j < len && buf_pos < size - 1; j++)
                    {
                        buffer[buf_pos++] = temp[j];
                    }
                    written += len;
                    break;
                }

            case 's':
                {
                    char* str = __builtin_va_arg(args, char*);
                    if (str)
                    {
                        const size_t len = strlen(str);

                        int pad_count = width > len ? width - len : 0;
                        for (int p = 0; p < pad_count && buf_pos < size - 1; p++)
                        {
                            buffer[buf_pos++] = ' ';
                        }
                        written += pad_count;

                        for (int j = 0; j < len && buf_pos < size - 1; j++)
                        {
                            buffer[buf_pos++] = str[j];
                        }
                        written += len;
                    }
                    break;
                }

            case 'c':
                {
                    char ch = (char)__builtin_va_arg(args, int);

                    int pad_count = width > 1 ? width - 1 : 0;
                    for (int p = 0; p < pad_count && buf_pos < size - 1; p++)
                    {
                        buffer[buf_pos++] = ' ';
                    }
                    written += pad_count;

                    if (buf_pos < size - 1)
                    {
                        buffer[buf_pos++] = ch;
                    }
                    written++;
                    break;
                }

            case '%':
                {
                    if (buf_pos < size - 1)
                    {
                        buffer[buf_pos++] = '%';
                    }
                    written++;
                    break;
                }

            default:
                // Unknown format specifier, just copy it
                if (buf_pos < size - 1)
                {
                    buffer[buf_pos++] = '%';
                }
                if (buf_pos < size - 1)
                {
                    buffer[buf_pos++] = format[i];
                }
                written += 2;
                break;
            }
        }
        else
        {
            // Regular character
            if (buf_pos < size - 1)
            {
                buffer[buf_pos++] = format[i];
            }
            written++;
        }
    }

    // Null terminate
    buffer[buf_pos] = '\0';

    __builtin_va_end(args);
    return written;
}

FILE* fopen(const char* path, const char* mode)
{
    if (!path || !mode) return NULL;

    int flags = 0;

    switch (mode[0])
    {
    case 'r': flags = O_RDONLY;
        break;
    case 'w': flags = O_WRONLY | O_CREAT | O_TRUNC;
        break;
    case 'a': flags = O_WRONLY | O_CREAT | O_APPEND;
        break;
    default: return NULL;
    }

    if (mode[1] == '+')
    {
        flags = O_RDWR | (flags & (O_CREAT | O_TRUNC | O_APPEND));
    }

    FILE_HANDLE handle = sys_open((uint64_t)path, flags, 0, 0, 0, 0);
    if (handle < 0) return NULL;

    FILE* f = malloc(sizeof(FILE));
    if (!f)
    {
        sys_close(handle, 0, 0, 0, 0, 0);
        return NULL;
    }

    f->handle = handle;
    f->error = 0;
    f->buffer = NULL;
    f->buf_size = 0;
    f->buf_pos = 0;

    return f;
}

int fclose(FILE* f)
{
    if (!f) return -1;
    fflush(f);
    int res = (int)sys_close(f->handle, 0, 0, 0, 0, 0);
    free(f);

    if (res < 0) {
        errno = res;
        return -1;
    }
    return 0;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* f)
{
    if (!f || !ptr) return 0;
    size_t total = size * nmemb;
    ssize_t read_bytes = sys_read(f->handle, (uint64_t)ptr, total, 0, 0, 0);
    if (read_bytes < 0)
    {
        f->error = 1;
        return 0;
    }
    return read_bytes / size;
}


size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* f)
{
    if (!f || !ptr) return 0;
    size_t total = size * nmemb;
    ssize_t written = sys_write(f->handle, (uint64_t)ptr, total, 0, 0, 0);
    if (written < 0)
    {
        f->error = 1;
        return 0;
    }
    return written / size;
}


// stub
int ferror(FILE* f)
{
    if (!f) return 1;
    return f->error;
}

// stub
int fflush(FILE* f)
{
    if (!f) return -1;
    return 0;
}


HANDLE open(const char* path, int flags)
{
    return sys_open((uint64_t)path, flags, 0, 0, 0, 0);
}

int close(HANDLE handle)
{
    return sys_close(handle, 0, 0, 0, 0, 0);
}

ssize_t read(HANDLE handle, void* buf, size_t count)
{
    return sys_read(handle, (uint64_t)buf, count, 0, 0, 0);
}

ssize_t write(HANDLE handle, const void* buf, size_t count)
{
    return sys_write(handle, (uint64_t)buf, count, 0, 0, 0);
}

int create(const char* path, int type)
{
    if (type == C_DIR)
    {
        return (int)sys_mkdir((uint64_t)path, 0, 0, 0, 0, 0);
    }
    return (int)sys_create((uint64_t)path, 0, 0, 0, 0, 0);
}

int creat(const char* path)
{
    return (int)sys_create((uint64_t)path, 0, 0, 0, 0, 0);
}

int unlink(const char* path)
{
    return (int)sys_unlink((uint64_t)path, 0, 0, 0, 0, 0);
}

int mkdir(const char* path)
{
    return (int)sys_mkdir((uint64_t)path, 0, 0, 0, 0, 0);
}

int rmdir(const char* path)
{
    return (int)sys_rmdir((uint64_t)path, 0, 0, 0, 0, 0);
}

ssize_t fseek(FILE_HANDLE stream, long offset, int whence)
{
    return sys_seek(stream, offset, whence, 0, 0, 0);
}

ssize_t ftell(FILE_HANDLE stream)
{
    return sys_seek(stream, 0, SEEK_CUR, 0, 0, 0);
}

int rewind(FILE_HANDLE stream)
{
    return (int)sys_seek(stream, 0, SEEK_SET, 0, 0, 0);
}

DIR_HANDLE opendir(const char* path)
{
    return sys_open((uint64_t)path, O_DIRECTORY, 0, 0, 0, 0);
}

int closedir(DIR_HANDLE handle)
{
    return sys_close(handle, 0, 0, 0, 0, 0);
}

ssize_t readdir(DIR_HANDLE handle, dirent_t* entry)
{
    if (!entry) return -1;
    return sys_readdir(handle, (uint64_t)entry, sizeof(dirent_t), 0, 0, 0);
}
