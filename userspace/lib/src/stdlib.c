// stdlib.c
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

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdbool.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

char** environ = NULL;
size_t env_count = 0;
size_t env_capacity = 0;

int* __errno_location(void) {
    return &errno;
}

static unsigned int gather_seed(void) {
    unsigned int seed = 0;

    const HANDLE hdl = vopen("/dev/urandom", O_RDONLY);
    if ((int64_t)hdl >= 0) {
        const bool ok = vread(hdl, &seed, sizeof(seed)) == sizeof(seed);
        vclose(hdl);
        if (ok && seed != 0) return seed;
    }

    seed ^= (unsigned int)getpid();
    seed ^= (unsigned int)(uintptr_t)&seed;
    seed ^= (unsigned int)(uintptr_t)gather_seed;

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) seed ^= ts.tv_sec;

    return seed ? seed : 1;
}

void init_environ(char** envp) {
    srand(gather_seed());

    size_t count = 0;
    while (envp[count]) count++;

    env_capacity = count + 16; // more space for variables by default
    environ = malloc(env_capacity * sizeof(char*));
    if (!environ) return;

    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(envp[i]) + 1;
        environ[i] = malloc(len);
        if (!environ[i]) return;
        memcpy(environ[i], envp[i], len);
    }

    environ[count] = NULL;
    env_count = count;
}

char* getenv(const char* name) {
    if (!name || !environ) return NULL;
    size_t name_len = strlen(name);

    for (size_t i = 0; environ[i]; i++) {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=') {
            return environ[i] + name_len + 1;
        }
    }
    return NULL;
}

int setenv(const char* name, const char* value, int overwrite) {
    if (!name || !value || strchr(name, '=')) return -1;
    size_t name_len = strlen(name);

    for (size_t i = 0; i < env_count; i++) {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=') {
            if (!overwrite) return 0;

            size_t new_len = name_len + 1 + strlen(value) + 1;
            char* new_entry = malloc(new_len);
            if (!new_entry) return -1;

            snprintf(new_entry, new_len, "%s=%s", name, value);

            free(environ[i]);
            environ[i] = new_entry;
            return 0;
        }
    }

    if (env_count + 1 >= env_capacity) {
        env_capacity = env_capacity * 2;
        char** new_environ = realloc(environ, env_capacity * sizeof(char*));
        if (!new_environ) return -1;
        environ = new_environ;
    }

    size_t new_len = name_len + 1 + strlen(value) + 1;
    char* new_entry = malloc(new_len);
    if (!new_entry) return -1;

    snprintf(new_entry, new_len, "%s=%s", name, value);

    environ[env_count] = new_entry;
    env_count++;
    environ[env_count] = NULL; // environ must be null-terminated

    return 0;
}

int unsetenv(const char* name) {
    if (!name || strchr(name, '=')) return -1;
    size_t name_len = strlen(name);

    for (size_t i = 0; i < env_count; i++) {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=') {
            free(environ[i]);

            for (size_t j = i; j < env_count - 1; j++) {
                environ[j] = environ[j + 1];
            }

            env_count--;
            environ[env_count] = NULL;

            return 0;
        }
    }
    return -1;
}

/* GETENV_S(buffer, size, name) */
static inline int GETENV_S(char* buffer, size_t buffer_size, const char* name) {
    char* val = getenv(name);
    if (!val) {
        errno = 22; /* EINVAL */
        return errno;
    }
    /* kopiere ins buffer */
    size_t i = 0;
    while (i < buffer_size - 1 && val[i]) {
        buffer[i] = val[i];
        i++;
    }
    buffer[i] = 0;
    return 0;
}

/* PUTENV_S(name, value) */
static inline int PUTENV_S(const char* name, const char* value) {
    if (!name || !value) {
        errno = 22; /* EINVAL */
        return errno;
    }
    int res = setenv(name, value, 1);
    if (res != 0) {
        errno = 12; /* ENOMEM, fallback */
        return errno;
    }
    return 0;
}

int atoi(const char* s) {
    if (!s) return 0;

    // Skip leading whitespace
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') {
        s++;
    }

    int sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    int result = 0;
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }

    return sign * result;
}

long atol(const char* s) {
    if (!s) return 0L;

    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') {
        s++;
    }

    long sign = 1L;
    if (*s == '-') {
        sign = -1L;
        s++;
    } else if (*s == '+') {
        s++;
    }

    long result = 0L;
    while (*s >= '0' && *s <= '9') {
        result = result * 10L + (long)(*s - '0');
        s++;
    }

    return sign * result;
}

double strtod(const char* str, char** endptr) {
    if (!str) {
        if (endptr) *endptr = (char*)str;
        return 0.0;
    }

    // skip whitespace
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r' || *str == '\f' || *str == '\v') str++;

    double result = 0.0;
    double sign = 1.0;

    // Sign
    if (*str == '-') {
        sign = -1.0;
        str++;
    } else if (*str == '+') {
        str++;
    }

    // NaN / Inf
    if (str[0] == 'i' || str[0] == 'I') {
        if (endptr) *endptr = (char*)str + 3;
        return sign * __builtin_inf();
    }
    if (str[0] == 'n' || str[0] == 'N') {
        if (endptr) *endptr = (char*)str + 3;
        return __builtin_nan("");
    }

    // Hex: 0x...
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
        double hex = 0.0;
        while ((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'f') || (*str >= 'A' && *str <= 'F')) {
            int digit;
            if (*str >= '0' && *str <= '9')
                digit = *str - '0';
            else if (*str >= 'a' && *str <= 'f')
                digit = *str - 'a' + 10;
            else
                digit = *str - 'A' + 10;
            hex = hex * 16.0 + digit;
            str++;
        }
        if (*str == '.') {
            str++;
            double frac = 1.0 / 16.0;
            while ((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'f') || (*str >= 'A' && *str <= 'F')) {
                int digit;
                if (*str >= '0' && *str <= '9')
                    digit = *str - '0';
                else if (*str >= 'a' && *str <= 'f')
                    digit = *str - 'a' + 10;
                else
                    digit = *str - 'A' + 10;
                hex += digit * frac;
                frac /= 16.0;
                str++;
            }
        }
        if (*str == 'p' || *str == 'P') {
            str++;
            int esign = 1;
            if (*str == '-') {
                esign = -1;
                str++;
            } else if (*str == '+')
                str++;
            int exp = 0;
            while (*str >= '0' && *str <= '9') exp = exp * 10 + (*str++ - '0');
            // scalbn: hex * 2^(esign*exp)
            hex = __builtin_scalbn(hex, esign * exp);
        }
        if (endptr) *endptr = (char*)str;
        return sign * hex;
    }

    // decimal part
    while (*str >= '0' && *str <= '9') result = result * 10.0 + (*str++ - '0');

    // decimal places
    if (*str == '.') {
        str++;
        double frac = 0.1;
        while (*str >= '0' && *str <= '9') {
            result += (*str++ - '0') * frac;
            frac *= 0.1;
        }
    }

    // exponent
    if (*str == 'e' || *str == 'E') {
        str++;
        int esign = 1;
        if (*str == '-') {
            esign = -1;
            str++;
        } else if (*str == '+')
            str++;
        int exp = 0;
        while (*str >= '0' && *str <= '9') exp = exp * 10 + (*str++ - '0');
        // result * 10^exp
        double base = 10.0;
        int e = esign * exp;
        if (e < 0) {
            base = 0.1;
            e = -e;
        }
        double factor = 1.0;
        for (int i = 0; i < e; i++) factor *= base;
        result *= factor;
    }

    if (endptr) *endptr = (char*)str;
    return sign * result;
}

double atof(const char* nptr) {
    return strtod(nptr, NULL);
}

float strtof(const char* str, char** endptr) {
    return (float)strtod(str, endptr);
}

long double strtold(const char* str, char** endptr) {
    return (long double)strtod(str, endptr);
}

unsigned long strtoul(const char* nptr, char** endptr, int base) {
    const char* s = nptr;
    unsigned long result = 0;
    int neg = 0;

    while (isspace((unsigned char)*s)) s++;

    if (*s == '+' || *s == '-') {
        if (*s == '-') neg = 1;
        s++;
    }

    if (base == 0) {
        if (*s == '0') {
            if ((s[1] == 'x' || s[1] == 'X') && isxdigit(s[2])) {
                base = 16;
                s += 2;
            } else {
                base = 8;
                s++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            s += 2;
        }
    }

    int any = 0;
    while (*s) {
        int digit;

        if (*s >= '0' && *s <= '9')
            digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z')
            digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z')
            digit = *s - 'A' + 10;
        else
            break;

        if (digit >= base) break;

        if (result > (ULONG_MAX - digit) / base) {
            errno = ERANGE;
            result = ULONG_MAX;
            any = 1;
            break;
        }

        result = result * base + digit;
        any = 1;
        s++;
    }

    if (!any) {
        errno = EINVAL;
        if (endptr) *endptr = (char*)nptr;
        return 0;
    }

    if (endptr) *endptr = (char*)s;

    if (neg) return (unsigned long)(-(long)result);

    return result;
}

long strtol(const char* nptr, char** endptr, int base) {
    const char* s = nptr;
    unsigned long result = 0;
    int neg = 0;
    int any = 0;

    while (isspace((unsigned char)*s)) s++;

    if (*s == '+' || *s == '-') {
        if (*s == '-') neg = 1;
        s++;
    }

    if (base == 0) {
        if (*s == '0') {
            if ((s[1] == 'x' || s[1] == 'X') && isxdigit((unsigned char)s[2])) {
                base = 16;
                s += 2;
            } else {
                base = 8;
                s++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            s += 2;
        }
    }

    if (base < 2 || base > 36) {
        errno = EINVAL;
        if (endptr) *endptr = (char*)nptr;
        return 0;
    }

    unsigned long cutoff = neg ? (unsigned long)-(LONG_MIN) : LONG_MAX;
    int cutlim = cutoff % base;
    cutoff /= base;

    while (*s) {
        int digit;

        if (*s >= '0' && *s <= '9')
            digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z')
            digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z')
            digit = *s - 'A' + 10;
        else
            break;

        if (digit >= base) break;

        if (any >= 0) {
            if (result > cutoff || (result == cutoff && digit > cutlim)) {
                any = -1;
                errno = ERANGE;
                result = neg ? (unsigned long)LONG_MIN : (unsigned long)LONG_MAX;
            } else {
                any = 1;
                result = result * base + digit;
            }
        }
        s++;
    }

    if (any == 0) {
        errno = EINVAL;
        if (endptr) *endptr = (char*)nptr;
        return 0;
    }

    if (endptr) {
        *endptr = (char*)s;
    }

    if (any < 0) {
        return (long)result;
    }

    return neg ? -(long)result : (long)result;
}

_Thread_local static unsigned long next = 0;

void srand(const unsigned int seed) {
    next = seed ? seed : 1;
}

int rand(void) {
    if (next == 0) {
        const HANDLE hdl = vopen("/dev/urandom", O_RDONLY);
        if ((int64_t)hdl >= 0) {
            unsigned int kernel_seed = 0;
            if (vread(hdl, &kernel_seed, sizeof(kernel_seed)) == sizeof(kernel_seed))
                next = kernel_seed ? kernel_seed : 1;
            vclose(hdl);
        }
        if (next == 0) next = 1;
    }

    next = next * 1103515245 + 12345;
    return (unsigned int)(next / 65536) % 2147483648;
}

_Noreturn void abort(void) {
    raise(SIGABRT);
    sys_exit(134, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

int system(const char* cmd) {
    if (!cmd) return 1;

    const char* argv[] = {cmd, NULL};
    int64_t rid = sys_spawn((uint64_t)cmd, (uint64_t)argv, 0, 0, 0, 0);
    if (rid < 0) return -1;

    int status = 0;
    sys_wait((uint64_t)rid, (uint64_t)&status, 0, 0, 0, 0);
    return status;
}

char* tmpnam(char* buf) {
    static char internal[L_tmpnam];
    static int counter = 0;

    char* dst = buf ? buf : internal;
    snprintf(dst, L_tmpnam, "/tmp/tmp%d", counter++);
    return dst;
}

#define MKSTEMP_RETRIES 100

static const char mkstemp_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

int mkstemp(char* tmpl) {
    const size_t len = strlen(tmpl);

    // Last 6 chars must be XXXXXX;
    if (len < 6 || strcmp(tmpl + len - 6, "XXXXXX") != 0) {
        errno = EINVAL;
        return -1;
    }

    char* const suffix = tmpl + len - 6;

    for (int attempt = 0; attempt < MKSTEMP_RETRIES; attempt++) {
        for (int i = 0; i < 6; i++) {
            const int r = rand() % (int)(sizeof(mkstemp_chars) - 1);
            suffix[i] = mkstemp_chars[r];
        }

        const int fd = open(tmpl, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (fd >= 0) return fd;

        if (errno != EEXIST) return -1;
    }

    errno = EEXIST;
    return -1;
}