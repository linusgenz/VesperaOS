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

#include <ctype.h>
#include <fflags.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sysstd.h>

#include "errno.h"

static size_t uint_to_str(uint64_t value, char* buffer, uint8_t base, bool prefix) {
    char temp[32];
    int i = 0;

    if (value == 0) {
        temp[i++] = '0';
    } else {
        while (value > 0) {
            const char* digits = "0123456789ABCDEF";
            temp[i++] = digits[value % base];
            value /= base;
        }
    }

    int j = 0;
    if (prefix && base == 16) {
        buffer[j++] = '0';
        buffer[j++] = 'x';
    }

    while (i--) {
        buffer[j++] = temp[i];
    }

    buffer[j] = '\0';

    return j;
}

static void float_to_str(float val, char* buf, int precision) {
    if (val < 0) {
        *buf++ = '-';
        val = -val;
    }

    uint32_t int_part = (uint32_t)val;
    float frac_part = val - (float)int_part;

    char int_buf[32];
    uint_to_str(int_part, int_buf, 10, false);
    char* p = int_buf;
    while (*p) {
        *buf++ = *p++;
    }

    *buf++ = '.';

    for (int i = 0; i < precision; i++) {
        frac_part *= 10.0f;
        int digit = (int)frac_part;
        *buf++ = '0' + digit;
        frac_part -= digit;
    }

    *buf = '\0';
}

static const char* skip_whitespace(const char* str) {
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }
    return str;
}

static bool parse_long(const char** str, long* result) {
    const char* s = *str;
    long val = 0;
    int sign = 1;
    bool has_digits = false;

    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    while (*s && isdigit((unsigned char)*s)) {
        const int digit = *s - '0';

        if (val > (LONG_MAX - digit) / 10) {
            return false;
        }

        val = val * 10 + digit;
        s++;
        has_digits = true;
    }

    if (!has_digits) {
        return false;
    }

    *result = sign * val;
    *str = s;
    return true;
}

int sscanf(const char* str, const char* format, ...) {
    if (!str || !format) {
        return -1;
    }

    va_list args;
    va_start(args, format);

    int matches = 0;
    const char* s = str;
    const char* f = format;

    while (*f && *s) {
        // Whitespace im Format überspringt beliebig viel Whitespace im String
        if (isspace((unsigned char)*f)) {
            s = skip_whitespace(s);
            f++;
            continue;
        }

        // Konvertierungsspezifikation
        if (*f == '%') {
            f++;

            if (*f == '%') {
                // Literales '%'
                if (*s != '%') {
                    break;
                }
                s++;
                f++;
                continue;
            }

            // Whitespace vor Konvertierung überspringen (außer bei %c)
            if (*f != 'c') {
                s = skip_whitespace(s);
            }

            if (*f == 'l') {
                f++;
                if (*f == 'd') {
                    // %ld - long int
                    long* ptr = va_arg(args, long*);
                    if (!ptr) {
                        va_end(args);
                        return -1;
                    }

                    long val = 0;
                    if (parse_long(&s, &val)) {
                        *ptr = val;
                        matches++;
                    } else {
                        break;  // Parsing fehlgeschlagen
                    }
                    f++;
                }
            } else if (*f == 'c') {
                // %c - einzelnes Zeichen (überspringt KEIN Whitespace)
                char* ptr = va_arg(args, char*);
                if (!ptr) {
                    va_end(args);
                    return -1;
                }

                if (*s) {
                    *ptr = *s;
                    s++;
                    matches++;
                } else {
                    break;  // Keine Zeichen mehr verfügbar
                }
                f++;
            } else {
                // Unbekannter Format-Spezifizierer
                break;
            }
        } else {
            // Literales Zeichen im Format muss übereinstimmen
            if (*f != *s) {
                break;
            }
            f++;
            s++;
        }
    }

    va_end(args);
    return matches;
}

int getchar(void) {
    char ch = 0;
    int ret = (int)sys_read(stdin, (uint64_t)&ch, 1, 0, 0, 0);
    if (ret <= 0) return -1;
    return (int)ch;
}

#define PRINTF_BUFFER_SIZE 1024

static char printf_buffer[PRINTF_BUFFER_SIZE];
static size_t printf_pos = 0;

static void flush_printf_buffer() {
    if (printf_pos > 0) {
        sys_write(stdout, (uint64_t)printf_buffer, printf_pos, 0, 0, 0);
        printf_pos = 0;
    }
}

typedef enum { SINK_CONSOLE, SINK_BUFFER } sink_type_t;

typedef struct {
    sink_type_t type;
    char* buf;
    size_t size;  // total size including NULL
    size_t pos;
    size_t written;
} sink_t;

static void sink_putc(sink_t* s, char c) {
    s->written++;
    if (s->type == SINK_CONSOLE) {
        printf_buffer[printf_pos++] = c;
        if (printf_pos == PRINTF_BUFFER_SIZE || c == '\n') {
            flush_printf_buffer();
        }
    } else {
        if (s->pos < s->size - 1) s->buf[s->pos++] = c;
    }
}

static void sink_puts(sink_t* s, const char* str) {
    if (!str) str = "<null>";
    while (*str) sink_putc(s, *str++);
}

// ─── padding helper ───────────────────────────────────────────────────────────

static void pad(sink_t* s, int count, char ch) {
    for (int i = 0; i < count; i++) sink_putc(s, ch);
}

static void vformat_write(sink_t* s, const char* fmt, __builtin_va_list args) {
    char c;
    while ((c = *fmt++) != '\0') {
        if (c != '%') {
            sink_putc(s, c);
            continue;
        }

        // flags
        bool left_align = false;
        bool zero_pad = false;

        bool parsing_flags = true;
        while (parsing_flags) {
            switch (*fmt) {
                case '-':
                    left_align = true;
                    fmt++;
                    break;
                case '0':
                    zero_pad = true;
                    fmt++;
                    break;
                default:
                    parsing_flags = false;
                    break;
            }
        }
        // Per C standard: '-' overrides '0'
        if (left_align) zero_pad = false;

        // width
        int min_width = 0;
        if (*fmt == '*') {
            min_width = __builtin_va_arg(args, int);
            fmt++;

            if (min_width < 0) {
                left_align = true;
                min_width = -min_width;
            }
        } else {
            while (*fmt >= '0' && *fmt <= '9') min_width = min_width * 10 + (*fmt++ - '0');
        }

        int precision = -1;  // -1 = not specified
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            while (*fmt >= '0' && *fmt <= '9') precision = precision * 10 + (*fmt++ - '0');
        }

        // length modifier
        bool is_long = false;
        bool is_long_long = false;
        if (*fmt == 'l') {
            fmt++;
            if (*fmt == 'l') {
                is_long_long = true;
                fmt++;
            } else
                is_long = true;
        }

        const char specifier = *fmt++;
        char tmp[64];
        size_t len = 0;
        int p = 0;

        switch (specifier) {
            case 's': {
                const char* str = __builtin_va_arg(args, const char*);
                if (!str) str = "<null>";
                len = strlen(str);
                p = (min_width > (int)len) ? min_width - (int)len : 0;

                if (!left_align) pad(s, p, ' ');
                sink_puts(s, str);
                if (left_align) pad(s, p, ' ');
                break;
            }
            case 'c': {
                char ch = (char)__builtin_va_arg(args, int);
                p = (min_width > 1) ? min_width - 1 : 0;

                if (!left_align) pad(s, p, ' ');
                sink_putc(s, ch);
                if (left_align) pad(s, p, ' ');
                break;
            }
            case 'd':
            case 'i': {
                int64_t val = (is_long_long || is_long) ? __builtin_va_arg(args, int64_t)
                                                        : (int64_t)__builtin_va_arg(args, int32_t);

                bool negative = val < 0;
                if (negative) val = -val;

                uint_to_str((uint64_t)val, tmp, 10, false);
                len = strlen(tmp);
                int total = (int)len + (negative ? 1 : 0);
                p = (min_width > total) ? min_width - total : 0;

                if (!left_align && !zero_pad) pad(s, p, ' ');
                if (negative) sink_putc(s, '-');
                if (!left_align && zero_pad) pad(s, p, '0');
                sink_puts(s, tmp);
                if (left_align) pad(s, p, ' ');
                break;
            }
            case 'u': {
                uint64_t val = (is_long_long || is_long) ? __builtin_va_arg(args, uint64_t)
                                                         : (uint64_t)__builtin_va_arg(args, uint32_t);

                uint_to_str(val, tmp, 10, false);
                len = strlen(tmp);
                p = (min_width > (int)len) ? min_width - (int)len : 0;

                if (!left_align) pad(s, p, zero_pad ? '0' : ' ');
                sink_puts(s, tmp);
                if (left_align) pad(s, p, ' ');
                break;
            }
            case 'x':
            case 'X': {
                uint64_t val = (is_long_long || is_long) ? __builtin_va_arg(args, uint64_t)
                                                         : (uint64_t)__builtin_va_arg(args, uint32_t);

                uint_to_str(val, tmp, 16, false);
                len = strlen(tmp);
                p = (min_width > (int)len) ? min_width - (int)len : 0;

                if (!left_align) pad(s, p, zero_pad ? '0' : ' ');
                sink_puts(s, tmp);
                if (left_align) pad(s, p, ' ');
                break;
            }
            case 'f': {
                double val = __builtin_va_arg(args, double);
                int prec = (precision >= 0) ? precision : 6;
                float_to_str((float)val, tmp, prec);
                len = strlen(tmp);
                p = (min_width > (int)len) ? min_width - (int)len : 0;
                if (!left_align) pad(s, p, zero_pad ? '0' : ' ');
                sink_puts(s, tmp);
                if (left_align) pad(s, p, ' ');
                break;
            }
            case 'o': {
                const uint64_t val = (is_long_long || is_long) ? __builtin_va_arg(args, uint64_t)
                                                         : (uint64_t)__builtin_va_arg(args, uint32_t);

                uint_to_str(val, tmp, 8, false);
                len = strlen(tmp);
                p = (min_width > (int)len) ? min_width - (int)len : 0;

                if (!left_align) pad(s, p, zero_pad ? '0' : ' ');
                sink_puts(s, tmp);
                if (left_align) pad(s, p, ' ');
                break;
            }
            case 'p': {
                uintptr_t val = __builtin_va_arg(args, uintptr_t);
                uint_to_str(val, tmp, 16, false);
                len = strlen(tmp) + 2;  // "0x" prefix counts toward width
                p = (min_width > (int)len) ? min_width - (int)len : 0;

                if (!left_align) pad(s, p, zero_pad ? '0' : ' ');
                sink_puts(s, "0x");
                sink_puts(s, tmp);
                if (left_align) pad(s, p, ' ');
                break;
            }
            case '%':
                sink_putc(s, '%');
                break;
            default:
                sink_putc(s, '%');
                sink_putc(s, specifier);
                break;
        }
    }
}

int puts(const char* s) {
    if (!s) return -1;

    sink_t sk = {.type = SINK_CONSOLE};
    sink_puts(&sk, s);
    sink_putc(&sk, '\n');
    flush_printf_buffer();
    return 0;
}

int putchar(int c) {
    sink_t s = {.type = SINK_CONSOLE};
    sink_putc(&s, (char)c);
    return c;
}

void printf(const char* fmt, ...) {
    sink_t s;
    s.type = SINK_CONSOLE;
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    vformat_write(&s, fmt, args);
    __builtin_va_end(args);
}

size_t snprintf(char* buffer, size_t size, const char* fmt, ...) {
    if (!buffer || !fmt || size == 0) return (size_t)-1;

    sink_t s = {
        .type = SINK_BUFFER,
        .buf = buffer,
        .size = size,
        .pos = 0,
        .written = 0,
    };

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    vformat_write(&s, fmt, args);
    __builtin_va_end(args);

    buffer[s.pos] = '\0';
    return s.written;  // total chars (same semantics as standard snprintf)
}

FILE* fopen(const char* path, const char* mode) {
    if (!path || !mode) return NULL;

    int flags = 0;

    switch (mode[0]) {
        case 'r':
            flags = O_RDONLY;
            break;
        case 'w':
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            break;
        case 'a':
            flags = O_WRONLY | O_CREAT | O_APPEND;
            break;
        default:
            return NULL;
    }

    if (mode[1] == '+') {
        flags = O_RDWR | (flags & (O_CREAT | O_TRUNC | O_APPEND));
    }

    FILE_HANDLE handle = sys_open((uint64_t)path, flags, 0, 0, 0, 0);
    if (handle < 0) return NULL;

    FILE* f = malloc(sizeof(FILE));
    if (!f) {
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

int fclose(FILE* f) {
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

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* f) {
    if (!f || !ptr) return 0;
    size_t total = size * nmemb;
    ssize_t read_bytes = sys_read(f->handle, (uint64_t)ptr, total, 0, 0, 0);
    if (read_bytes < 0) {
        f->error = 1;
        return 0;
    }
    return read_bytes / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* f) {
    if (!f || !ptr) return 0;
    size_t total = size * nmemb;
    ssize_t written = sys_write(f->handle, (uint64_t)ptr, total, 0, 0, 0);
    if (written < 0) {
        f->error = 1;
        return 0;
    }
    return written / size;
}

// stub
int ferror(FILE* f) {
    if (!f) return 1;
    return f->error;
}

int fflush(FILE* f) {
    if (f == NULL || (FILE_HANDLE)(uintptr_t)f == stdout) {
        flush_printf_buffer();
        return 0;
    }
    return 0;
}

HANDLE open(const char* path, int flags) {
    return sys_open((uint64_t)path, flags, 0, 0, 0, 0);
}

int close(HANDLE handle) {
    return sys_close(handle, 0, 0, 0, 0, 0);
}

ssize_t read(HANDLE handle, void* buf, size_t count) {
    return sys_read(handle, (uint64_t)buf, count, 0, 0, 0);
}

ssize_t write(HANDLE handle, const void* buf, size_t count) {
    return sys_write(handle, (uint64_t)buf, count, 0, 0, 0);
}

int create(const char* path, int type) {
    if (type == C_DIR) {
        return (int)sys_mkdir((uint64_t)path, 0, 0, 0, 0, 0);
    }
    return (int)sys_create((uint64_t)path, 0, 0, 0, 0, 0);
}

int creat(const char* path) {
    return (int)sys_create((uint64_t)path, 0, 0, 0, 0, 0);
}

int unlink(const char* path) {
    return (int)sys_unlink((uint64_t)path, 0, 0, 0, 0, 0);
}

int mkdir(const char* path) {
    return (int)sys_mkdir((uint64_t)path, 0, 0, 0, 0, 0);
}

int rmdir(const char* path) {
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

DIR_HANDLE opendir(const char* path) {
    return sys_open((uint64_t)path, O_DIRECTORY, 0, 0, 0, 0);
}

int closedir(DIR_HANDLE handle) {
    return sys_close(handle, 0, 0, 0, 0, 0);
}

ssize_t readdir(DIR_HANDLE handle, dirent_t* entry) {
    if (!entry) return -1;
    return sys_readdir(handle, (uint64_t)entry, sizeof(dirent_t), 0, 0, 0);
}

int chdir(const char* path) {
    return (int)sys_chdir((uint64_t)path, 0, 0, 0, 0, 0);
}

int getcwd(char* buf, size_t size) {
    if (!buf || size == 0) {
        errno = EINVAL;
        return -EINVAL;
    }
    int64_t ret = sys_getcwd((uint64_t)buf, size, 0, 0, 0, 0);
    if (ret < 0) {
        errno = ret;
        return ret;
    }
    return 0;
}