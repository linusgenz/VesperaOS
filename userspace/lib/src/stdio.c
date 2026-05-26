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
#include <vespera/handles.h>

#include "errno.h"

static FILE __stdin_file;
static FILE __stdout_file;
static FILE __stderr_file;

FILE* stdin = &__stdin_file;
FILE* stdout = &__stdout_file;
FILE* stderr = &__stderr_file;

void __stdio_init(FILE_HANDLE in, FILE_HANDLE out, FILE_HANDLE err) {
    __stdin_file = (FILE){.handle = in, .error = 0, .eof = 0, .unget_char = -1};
    __stdout_file = (FILE){.handle = out, .error = 0, .eof = 0, .unget_char = -1};
    __stderr_file = (FILE){.handle = err, .error = 0, .eof = 0, .unget_char = -1};
}

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

static bool parse_uint64(const char** s, uint64_t* result, int base) {
    const char* p = *s;
    uint64_t val = 0;
    bool has_digits = false;

    while (*p) {
        int digit;
        if (*p >= '0' && *p <= '9')
            digit = *p - '0';
        else if (*p >= 'a' && *p <= 'f')
            digit = 10 + (*p - 'a');
        else if (*p >= 'A' && *p <= 'F')
            digit = 10 + (*p - 'A');
        else
            break;

        if (digit >= base) break;

        /* Overflow check */
        if (val > (UINT64_MAX - (uint64_t)digit) / (uint64_t)base) return false;

        val = val * (uint64_t)base + (uint64_t)digit;
        p++;
        has_digits = true;
    }

    if (!has_digits) return false;
    *result = val;
    *s = p;
    return true;
}

static bool parse_int64(const char** s, int64_t* result) {
    const char* p = *s;
    int sign = 1;

    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    uint64_t uval = 0;
    if (!parse_uint64(&p, &uval, 10)) return false;

    if (uval > (uint64_t)INT64_MAX + (sign < 0 ? 1u : 0u)) return false;

    *result = (int64_t)uval * sign;
    *s = p;
    return true;
}

static bool parse_float(const char** s, double* result) {
    const char* p = *s;
    double sign = 1.0;

    if (*p == '-') {
        sign = -1.0;
        p++;
    } else if (*p == '+') {
        p++;
    }

    /* Integer part */
    double val = 0.0;
    bool has_digits = false;
    while (*p >= '0' && *p <= '9') {
        val = val * 10.0 + (*p - '0');
        p++;
        has_digits = true;
    }

    /* Fractional part */
    if (*p == '.') {
        p++;
        double frac = 0.1;
        while (*p >= '0' && *p <= '9') {
            val += (*p - '0') * frac;
            frac *= 0.1;
            p++;
            has_digits = true;
        }
    }

    if (!has_digits) return false;
    *result = sign * val;
    *s = p;
    return true;
}

/* ── Core ────────────────────────────────────────────────────────────── */

static int vsscanf_internal(const char* str, const char* fmt, va_list args) {
    if (!str || !fmt) return -1;

    int matches = 0;
    const char* s = str;

    while (*fmt) {
        if (isspace((unsigned char)*fmt)) {
            s = skip_whitespace(s);
            fmt++;
            continue;
        }

        if (*fmt != '%') {
            if (*s != *fmt) break;
            s++;
            fmt++;
            continue;
        }
        fmt++;

        /* %% */
        if (*fmt == '%') {
            if (*s != '%') break;
            s++;
            fmt++;
            continue;
        }

        bool suppress = false;
        while (*fmt == '*') {
            suppress = true;
            fmt++;
        }

        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');

        bool is_long = false;
        bool is_long_long = false;
        bool is_short = false;

        if (*fmt == 'h') {
            is_short = true;
            fmt++;
        } else if (*fmt == 'l') {
            fmt++;
            if (*fmt == 'l') {
                is_long_long = true;
                fmt++;
            } else
                is_long = true;
        } else if (*fmt == 'z') {
            is_long_long = (sizeof(size_t) == 8);
            is_long = (sizeof(size_t) == 4);
            fmt++;
        }

        const char spec = *fmt++;

        /* %c does NOT skip whitespace */
        if (spec != 'c') s = skip_whitespace(s);
        if (!*s && spec != 'n') break;

        switch (spec) {
            case 'd':
            case 'i': {
                int64_t val = 0;
                const char* tmp = s;
                if (!parse_int64(&tmp, &val)) goto done;

                if (width > 0 && (int)(tmp - s) > width) {
                    /* Re-parse with capped input */
                    char wbuf[32];
                    int wlen = width < 31 ? width : 31;
                    memcpy(wbuf, s, (size_t)wlen);
                    wbuf[wlen] = '\0';
                    const char* wp = wbuf;
                    if (!parse_int64(&wp, &val)) goto done;
                    s += (wp - wbuf);
                } else {
                    s = tmp;
                }

                if (!suppress) {
                    if (is_long_long)
                        *va_arg(args, long long*) = (long long)val;
                    else if (is_long)
                        *va_arg(args, long*) = (long)val;
                    else if (is_short)
                        *va_arg(args, short*) = (short)val;
                    else
                        *va_arg(args, int*) = (int)val;
                    matches++;
                }
                break;
            }

            case 'u': {
                uint64_t val = 0;
                const char* tmp = s;
                if (!parse_uint64(&tmp, &val, 10)) goto done;
                s = tmp;

                if (!suppress) {
                    if (is_long_long)
                        *va_arg(args, unsigned long long*) = (unsigned long long)val;
                    else if (is_long)
                        *va_arg(args, unsigned long*) = (unsigned long)val;
                    else if (is_short)
                        *va_arg(args, unsigned short*) = (unsigned short)val;
                    else
                        *va_arg(args, unsigned int*) = (unsigned int)val;
                    matches++;
                }
                break;
            }

            case 'x':
            case 'X': {
                /* Skip optional 0x prefix */
                if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;

                uint64_t val = 0;
                const char* tmp = s;
                if (!parse_uint64(&tmp, &val, 16)) goto done;
                s = tmp;

                if (!suppress) {
                    if (is_long_long)
                        *va_arg(args, unsigned long long*) = (unsigned long long)val;
                    else if (is_long)
                        *va_arg(args, unsigned long*) = (unsigned long)val;
                    else
                        *va_arg(args, unsigned int*) = (unsigned int)val;
                    matches++;
                }
                break;
            }

            case 'o': {
                uint64_t val = 0;
                const char* tmp = s;
                if (!parse_uint64(&tmp, &val, 8)) goto done;
                s = tmp;

                if (!suppress) {
                    if (is_long_long)
                        *va_arg(args, unsigned long long*) = (unsigned long long)val;
                    else if (is_long)
                        *va_arg(args, unsigned long*) = (unsigned long)val;
                    else
                        *va_arg(args, unsigned int*) = (unsigned int)val;
                    matches++;
                }
                break;
            }

            case 'p': {
                if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
                uint64_t val = 0;
                const char* tmp = s;
                if (!parse_uint64(&tmp, &val, 16)) goto done;
                s = tmp;
                if (!suppress) {
                    *va_arg(args, void**) = (void*)(uintptr_t)val;
                    matches++;
                }
                break;
            }

            case 'f':
            case 'g':
            case 'G':
            case 'e':
            case 'E': {
                double val = 0.0;
                const char* tmp = s;
                if (!parse_float(&tmp, &val)) goto done;
                s = tmp;
                if (!suppress) {
                    /* vformat reads double, always store as double/float */
                    if (is_long)
                        *va_arg(args, double*) = val;
                    else
                        *va_arg(args, float*) = (float)val;
                    matches++;
                }
                break;
            }

            case 's': {
                char* ptr = suppress ? NULL : va_arg(args, char*);
                int n = 0;

                while (*s && !isspace((unsigned char)*s)) {
                    if (width > 0 && n >= width) break;
                    if (ptr) *ptr++ = *s;
                    s++;
                    n++;
                }

                if (n == 0) goto done;
                if (ptr) {
                    *ptr = '\0';
                }
                if (!suppress) matches++;
                break;
            }

            case 'c': {
                /* With width: read exactly width chars into array (no NUL) */
                int count = (width > 0) ? width : 1;
                char* ptr = suppress ? NULL : va_arg(args, char*);

                for (int k = 0; k < count; k++) {
                    if (!*s) goto done;
                    if (ptr) *ptr++ = *s;
                    s++;
                }
                if (!suppress) matches++;
                break;
            }

            case 'n': {
                if (!suppress) {
                    if (is_long_long)
                        *va_arg(args, long long*) = (long long)(s - str);
                    else if (is_long)
                        *va_arg(args, long*) = (long)(s - str);
                    else
                        *va_arg(args, int*) = (int)(s - str);
                }
                break;
            }

            default:
                goto done;
        }
    }

done:
    return matches;
}

int sscanf(const char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsscanf_internal(str, format, args);
    va_end(args);
    return ret;
}

int fscanf(FILE* f, const char* format, ...) {
    if (!f || !format) return -1;

    char buf[1024];
    size_t pos = 0;

    while (pos < sizeof(buf) - 1) {
        int c = fgetc(f);
        if (c == EOF) break;

        buf[pos++] = (char)c;

        if (isspace(c)) break;
    }

    buf[pos] = '\0';

    va_list args;
    va_start(args, format);
    int ret = vsscanf_internal(buf, format, args);
    va_end(args);

    return ret;
}

int getchar(void) {
    char ch = 0;
    int ret = (int)sys_read(HANDLE_STDIN, (uint64_t)&ch, 1, 0, 0, 0);
    if (ret <= 0) return EOF;
    return (int)ch;
}

int fgetc(FILE* f) {
    if (!f) return -1;
    if (f->unget_char != -1) {
        int c = f->unget_char;
        f->unget_char = -1;
        return c;
    }
    if (f->eof) return -1;
    unsigned char c;
    ssize_t r = sys_read(f->handle, (uint64_t)&c, 1, 0, 0, 0);
    if (r == 0) {
        f->eof = 1;
        return -1;
    }
    if (r < 0) {
        f->error = 1;
        return -1;
    }
    return (int)c;
}

int ungetc(int c, FILE* f) {
    if (!f || c == -1) return -1;
    f->unget_char = (unsigned char)c;
    f->eof = 0;
    return (unsigned char)c;
}

#define PRINTF_BUFFER_SIZE 1024

static char printf_buffer[PRINTF_BUFFER_SIZE];
static size_t printf_pos = 0;

static void flush_printf_buffer() {
    if (printf_pos > 0) {
        sys_write(HANDLE_STDOUT, (uint64_t)printf_buffer, printf_pos, 0, 0, 0);
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

                if (precision >= 0 && (int)len > precision) len = (size_t)precision;

                p = (min_width > (int)len) ? min_width - (int)len : 0;

                if (!left_align) pad(s, p, ' ');
                for (size_t k = 0; k < len; k++) sink_putc(s, str[k]);
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
            case 'g':
            case 'G':
            case 'e':
            case 'E': {
                double val = __builtin_va_arg(args, double);
                int prec = (precision >= 0) ? precision : 6;
                if (prec == 0) prec = 1;

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

int vfprintf(FILE* f, const char* fmt, va_list args) {
    if (!f || !fmt) return -1;

    char buf[4096];
    sink_t s = {
        .type = SINK_BUFFER,
        .buf = buf,
        .size = sizeof(buf),
        .pos = 0,
        .written = 0,
    };

    vformat_write(&s, fmt, args);
    buf[s.pos] = '\0';

    ssize_t written = sys_write(f->handle, (uint64_t)buf, s.pos, 0, 0, 0);
    if (written < 0) {
        f->error = 1;
        return -1;
    }
    return (int)s.written;
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

size_t snprintf(char* buffer, size_t size, const char* fmt, ...) {
    if (!buffer || !fmt || size == 0) return (size_t)-1;
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buffer, size, fmt, args);
    va_end(args);
    return (size_t)ret;
}

int vsnprintf(char* buffer, size_t size, const char* fmt, va_list args) {
    if (!buffer || !fmt || size == 0) return -1;

    sink_t s = {
        .type    = SINK_BUFFER,
        .buf     = buffer,
        .size    = size,
        .pos     = 0,
        .written = 0,
    };

    vformat_write(&s, fmt, args);
    buffer[s.pos] = '\0';
    return (int)s.written;
}

char* fgets(char* buf, int n, FILE* f) {
    if (!buf || n <= 0 || !f) return NULL;
    int i = 0;
    while (i < n - 1) {
        char c;
        ssize_t r = sys_read(f->handle, (uint64_t)&c, 1, 0, 0, 0);
        if (r == 0) {
            f->eof = 1;
            break;
        }
        if (r < 0) {
            f->error = 1;
            return NULL;
        }
        buf[i++] = c;
        if (c == '\n') break;
    }
    if (i == 0) return NULL;
    buf[i] = '\0';
    return buf;
}

int fprintf(FILE* f, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vfprintf(f, fmt, args);
    va_end(args);
    return ret;
}

int vprintf(const char* fmt, va_list args) {
    sink_t s = {.type = SINK_CONSOLE};
    vformat_write(&s, fmt, args);
    flush_printf_buffer();
    return (int)s.written;
}

void printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
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

    int64_t res = sys_open((uint64_t)path, flags, 0, 0, 0, 0);
    if (res < 0) {
        errno = res;
        return NULL;
    };
    const FILE_HANDLE handle = res;

    FILE* f = malloc(sizeof(FILE));
    if (!f) {
        sys_close(handle, 0, 0, 0, 0, 0);
        return NULL;
    }

    f->handle = handle;
    f->error = 0;
    f->eof = 0;
    f->unget_char = -1;
    f->buffer = NULL;
    f->buf_size = 0;
    f->buf_pos = 0;

    return f;
}

FILE* freopen(const char* path, const char* mode, FILE* f) {
    if (!f) return NULL;

    fflush(f);
    sys_close(f->handle, 0, 0, 0, 0, 0);

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
    if (mode[1] == '+') flags = O_RDWR | (flags & (O_CREAT | O_TRUNC | O_APPEND));

    FILE_HANDLE handle = sys_open((uint64_t)path, flags, 0, 0, 0, 0);
    if (handle < 0) return NULL;

    f->handle = handle;
    f->error = 0;
    f->eof = 0;
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
    if (read_bytes == 0) {
        f->eof = 1;
        return 0;
    }
    if (read_bytes < 0) {
        f->error = 1;
        return 0;
    }
    return read_bytes / size;
}

#define FILE_BUFFER_SIZE 4096

static int ensure_buffer(FILE* f) {
    if (!f->buffer) {
        f->buffer = malloc(FILE_BUFFER_SIZE);
        if (!f->buffer) {
            f->error = 1;
            return -1;
        }
        f->buf_size = FILE_BUFFER_SIZE;
        f->buf_pos = 0;
    }
    return 0;
}

static int flush_file_buffer(FILE* f) {
    if (!f || !f->buffer || f->buf_pos == 0) return 0;

    ssize_t written = sys_write(f->handle, (uint64_t)f->buffer, f->buf_pos, 0, 0, 0);

    if (written < 0) {
        f->error = 1;
        return -1;
    }

    f->buf_pos = 0;
    return 0;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* f) {
    if (!f || !ptr) return 0;

    size_t total = size * nmemb;
    const unsigned char* p = (const unsigned char*)ptr;

    if (total >= f->buf_size) {
        if (flush_file_buffer(f) < 0) return 0;

        ssize_t written = sys_write(f->handle, (uint64_t)ptr, total, 0, 0, 0);
        if (written < 0) {
            f->error = 1;
            return 0;
        }
        return written / size;
    }

    if (ensure_buffer(f) < 0) return 0;

    size_t written_total = 0;

    while (written_total < total) {
        size_t space = f->buf_size - f->buf_pos;

        if (space == 0) {
            if (flush_file_buffer(f) < 0) return written_total / size;
            space = f->buf_size;
        }

        size_t to_copy = total - written_total;
        if (to_copy > space) to_copy = space;

        memcpy(f->buffer + f->buf_pos, p + written_total, to_copy);
        f->buf_pos += to_copy;
        written_total += to_copy;
    }

    return nmemb;
}

int fputc(int c, FILE* f) {
    if (!f) return EOF;

    if (ensure_buffer(f) < 0) return EOF;

    if (f->buf_pos >= f->buf_size) {
        if (flush_file_buffer(f) < 0) return EOF;
    }

    f->buffer[f->buf_pos++] = (char)c;

    if (c == '\n') {
        flush_file_buffer(f);
    }

    return (unsigned char)c;
}

int fputs(const char* s, FILE* f) {
    if (!s || !f) return EOF;

    size_t len = strlen(s);

    if (ensure_buffer(f) < 0) return EOF;

    if (len >= f->buf_size) {
        if (flush_file_buffer(f) < 0) return EOF;

        ssize_t written = sys_write(f->handle, (uint64_t)s, len, 0, 0, 0);
        if (written < 0) {
            f->error = 1;
            return EOF;
        }
        return 0;
    }

    if (f->buf_pos + len > f->buf_size) {
        if (flush_file_buffer(f) < 0) return EOF;
    }

    memcpy(f->buffer + f->buf_pos, s, len);
    f->buf_pos += len;

    return 0;
}

// stub
int ferror(FILE* f) {
    if (!f) return 1;
    return f->error;
}

int feof(FILE* f) {
    if (!f) return 1;
    return f->eof;
}

int fflush(FILE* f) {
    if (f == NULL || f == stdout) {
        flush_printf_buffer();
        return 0;
    }

    if (flush_file_buffer(f) < 0) return EOF;

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

int fseek(FILE* f, long offset, int whence) {
    if (!f) return -1;
    f->unget_char = -1;  // ← neu
    ssize_t ret = sys_seek(f->handle, offset, whence, 0, 0, 0);
    if (ret >= 0) f->eof = 0;
    return (ret < 0) ? -1 : 0;
}

ssize_t ftell(FILE* f) {
    if (!f) return -1;
    return sys_seek(f->handle, 0, SEEK_CUR, 0, 0, 0);
}

void rewind(FILE* f) {
    if (!f) return;
    f->unget_char = -1;
    sys_seek(f->handle, 0, SEEK_SET, 0, 0, 0);
    f->eof = 0;
    f->error = 0;
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

FILE* tmpfile(void) {
    static int counter = 0;
    char path[32];
    snprintf(path, sizeof(path), "/tmp/tmp%d", counter++);

    int ret = creat(path);
    if (ret < 0) return NULL;

    FILE* f = fopen(path, "w+");
    unlink(path);
    return f;
}

void clearerr(FILE* f) {
    if (!f) return;
    f->error = 0;
    f->eof = 0;
    f->unget_char = -1;
}

int setvbuf(FILE* f, char* buf, int mode, size_t size) {
    if (!f) return -1;

    (void)buf;
    (void)mode;
    (void)size;
    return 0;
}

int remove(const char* path) {
    if (!path) return -1;
    int ret = (int)sys_unlink((uint64_t)path, 0, 0, 0, 0, 0);
    if (ret < 0) ret = (int)sys_rmdir((uint64_t)path, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

int rename(const char* oldpath, const char* newpath) {
    if (!oldpath || !newpath) return -1;
    int ret = (int)sys_rename((uint64_t)oldpath, (uint64_t)newpath, 0, 0, 0, 0);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}