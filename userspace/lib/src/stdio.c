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
#include <stdbool.h>

int putchar(int c) {
    char ch = (char) c;
    return (int) sys_write(stdout, (uint64_t) &ch, 1, 0, 0, 0);
}

int puts(const char *s) {
    if (!s) return -1;
    size_t len = 0;
    while (s[len]) len++;
    int ret = (int) sys_write(stdout, (uint64_t) s, len, 0, 0, 0);
    return ret;
}

int getchar(void) {
    char ch;
    int ret = (int) sys_read(stdin, (uint64_t) &ch, 1, 0, 0, 0);
    if (ret <= 0) return -1;
    return (int) ch;
}

void printf(const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    char chr;
    while ((chr = *fmt++) != 0) {
        if (chr == '%') {
            // Flags & Width
            bool long_long = false;
            bool long_flag = false;
            char pad_char = ' ';
            int min_width = 0;

            // Padding: z. B. %02x → '0' erkannt
            if (*fmt == '0') {
                pad_char = '0';
                fmt++;
            }

            // Breite (z. B. 2, 4, 8, etc.)
            while (*fmt >= '0' && *fmt <= '9') {
                min_width = min_width * 10 + (*fmt - '0');
                fmt++;
            }

            // Länge: l / ll
            if (*fmt == 'l') {
                fmt++;
                if (*fmt == 'l') {
                    long_long = true;
                    fmt++;
                } else {
                    long_flag = true;
                }
            }

            char specifier = *fmt++;
            char buffer[64];

            switch (specifier) {
                case 's': {
                    const char *str = __builtin_va_arg(args, const char*);
                    puts(str ? str : "<null>");
                    break;
                }
                case 'u':
                case 'x': {
                    uint64_t val = (long_long || long_flag)
                                       ? __builtin_va_arg(args, uint64_t)
                                       : __builtin_va_arg(args, uint32_t);
                    int base = (specifier == 'x') ? 16 : 10;
                    uint_to_str(val, buffer, base, false);

                    // Padding manuell
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        putchar(pad_char);

                    puts(buffer);
                    break;
                }
                case 'c': {
                    int val = __builtin_va_arg(args, int);
                    putchar((char) val);
                    break;
                }
                case 'd': {
                    int64_t val = (long_long || long_flag)
                                      ? __builtin_va_arg(args, int64_t)
                                      : __builtin_va_arg(args, int32_t);
                    if (val < 0) {
                        putchar('-');
                        val = -val;
                    }
                    uint_to_str((uint64_t) val, buffer, 10, false);
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        putchar(pad_char);
                    puts(buffer);
                    break;
                }
                case 'f': {
                    // Neuer Float-Support
                    double val = __builtin_va_arg(args, double);
                    float_to_str((float) val, buffer, 6); // 6 Nachkommastellen Standard
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        putchar(pad_char);
                    puts(buffer);
                    break;
                }
                case 'p': {
                    uintptr_t val = __builtin_va_arg(args, uintptr_t);
                    puts("0x");
                    uint_to_str(val, buffer, 16, false);
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        putchar('0');
                    puts(buffer);
                    break;
                }
                case '%':
                    putchar('%');
                    break;
                default:
                    putchar('%');
                    putchar(specifier);
                    break;
            }
        } else {
            putchar(chr);
        }
    }
    __builtin_va_end(args);
}


size_t snprintf(char *buffer, size_t size, const char *format, ...) {
    if (!buffer || !format || size == 0) {
        return -1;
    }

    __builtin_va_list args;
    __builtin_va_start(args, format);

    size_t buf_pos = 0;
    size_t written = 0;

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%' && format[i + 1] != '\0') {
            i++; // Skip '%'

            switch (format[i]) {
                case 'd': {
                    int val = __builtin_va_arg(args, int);
                    char temp[32];
                    size_t len = uint_to_str(val, temp, 10, false);

                    for (int j = 0; j < len && buf_pos < size - 1; j++) {
                        buffer[buf_pos++] = temp[j];
                    }
                    written += len;
                    break;
                }

                case 'x': {
                    int val = __builtin_va_arg(args, int);
                    char temp[32];
                    const size_t len = uint_to_str(val, temp, 16, false);

                    for (int j = 0; j < len && buf_pos < size - 1; j++) {
                        buffer[buf_pos++] = temp[j];
                    }
                    written += len;
                    break;
                }

                case 's': {
                    char *str = __builtin_va_arg(args, char*);
                    if (str) {
                        const size_t len = strlen(str);
                        for (int j = 0; j < len && buf_pos < size - 1; j++) {
                            buffer[buf_pos++] = str[j];
                        }
                        written += len;
                    }
                    break;
                }

                case 'c': {
                    char ch = (char)__builtin_va_arg(args, int);
                    if (buf_pos < size - 1) {
                        buffer[buf_pos++] = ch;
                    }
                    written++;
                    break;
                }

                case 'l': {
                    if (format[i + 1] == 'l') {
                        i++;
                        if (format[i + 1] == 'u') {
                            i++;
                            unsigned long long val = __builtin_va_arg(args, unsigned long long);
                            char temp[32];
                            const size_t len = uint_to_str(val, temp, 10, false);

                            for (int j = 0; j < len && buf_pos < size - 1; j++) {
                                buffer[buf_pos++] = temp[j];
                            }
                            written += len;
                        }
                    }
                    break;
                }

                case '%': {
                    if (buf_pos < size - 1) {
                        buffer[buf_pos++] = '%';
                    }
                    written++;
                    break;
                }

                default:
                    // Unknown format specifier, just copy it
                    if (buf_pos < size - 1) {
                        buffer[buf_pos++] = '%';
                    }
                    if (buf_pos < size - 1) {
                        buffer[buf_pos++] = format[i];
                    }
                    written += 2;
                    break;
            }
        } else {
            // Regular character
            if (buf_pos < size - 1) {
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

FILE_HANDLE fopen(const char *path, int flags) {
    return sys_open((uint64_t) path, flags, 0, 0, 0, 0);
}

int fclose(FILE_HANDLE handle) {
    return sys_close(handle, 0, 0, 0, 0, 0);
}

ssize_t fread(FILE_HANDLE handle, void *buf, size_t count) {
    return sys_read(handle, (uint64_t) buf, count, 0, 0, 0);
}

ssize_t fwrite(FILE_HANDLE handle, const void *buf, size_t count) {
    return sys_write(handle, (uint64_t) buf, count, 0, 0, 0);
}

HANDLE_ID open(const char *path, int flags) {
    return sys_open((uint64_t) path, flags, 0, 0, 0, 0);
}

int close(HANDLE_ID handle) {
    return sys_close(handle, 0, 0, 0, 0, 0);
}

ssize_t read(HANDLE_ID handle, void *buf, size_t count) {
    return sys_read(handle, (uint64_t) buf, count, 0, 0, 0);
}

ssize_t write(HANDLE_ID handle, const void *buf, size_t count) {
    return sys_write(handle, (uint64_t) buf, count, 0, 0, 0);
}

int create(const char *path, int type) {
    if (type == C_DIR) {
        return (int)sys_mkdir((uint64_t)path, 0, 0, 0, 0, 0);
    }
    return (int)sys_create((uint64_t)path, 0, 0, 0, 0, 0);
}

int creat(const char *path) {
    return (int)sys_create((uint64_t)path, 0, 0, 0, 0, 0);
}

int unlink(const char *path) {
    return (int)sys_unlink((uint64_t)path, 0, 0, 0, 0, 0);
}

int mkdir(const char *path) {
    return (int)sys_mkdir((uint64_t)path, 0, 0, 0, 0, 0);
}

int rmdir(const char *path) {
    return (int)sys_rmdir((uint64_t)path, 0, 0, 0, 0, 0);
}

ssize_t fseek(FILE_HANDLE stream, long offset, int whence) {
    return sys_seek(stream, offset, whence, 0, 0, 0);
}

ssize_t ftell(FILE_HANDLE stream) {
    return sys_seek(stream, 0, SEEK_CUR, 0, 0, 0);
}

int rewind(FILE_HANDLE stream) {
    return (int)sys_seek(stream, 0, SEEK_SET, 0, 0, 0);
}

DIR_HANDLE opendir(const char *path) {
    return sys_open((uint64_t) path, O_DIRECTORY, 0, 0, 0, 0);
}

int closedir(DIR_HANDLE handle) {
    return sys_close(handle, 0, 0, 0, 0, 0);
}

ssize_t readdir(DIR_HANDLE handle, dirent_t *entry) {
    if (!entry) return -1;
    return sys_readdir(handle, (uint64_t)entry, sizeof(dirent_t), 0, 0, 0);
}