// time.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 29.03.26.
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
#ifndef VESPERAOS_TIME_H
#define VESPERAOS_TIME_H

#include <stddef.h>
#include <stdint.h>
#include <vespera/time.h>

typedef uint32_t time_t;

struct tm {
    int tm_sec;    ///< Seconds          [0, 60]
    int tm_min;    ///< Minutes          [0, 59]
    int tm_hour;   ///< Hours            [0, 23]
    int tm_mday;   ///< Day of month     [1, 31]
    int tm_mon;    ///< Month            [0, 11]  (0 = January)
    int tm_year;   ///< Years since 1900
    int tm_wday;   ///< Day of week      [0, 6]   (0 = Sunday)
    int tm_yday;   ///< Day of year      [0, 365]
    int tm_isdst;  ///< Daylight saving  (always 0 on VesperaOS)
};

/**
 * @brief Get the current time of the specified clock.
 *
 * @param clk_id  Clock source (CLOCK_REALTIME, CLOCK_MONOTONIC, ...)
 * @param ts      Output: filled with seconds and nanoseconds.
 * @return        0 on success, negative errno on failure.
 */
int64_t clock_gettime(clockid_t clk_id, timespec_t* ts);

/**
 * @brief Decompose a Unix timestamp into a broken-down UTC time.
 *
 * @param timep  Pointer to a time_t value (Unix seconds since epoch).
 * @return       Pointer to a statically allocated struct tm, or NULL on error.
 *               The returned pointer is overwritten on each call.
 */
struct tm* gmtime(const time_t* timep);

/**
 * @brief Format a broken-down time into a string.
 *
 * Supported conversion specifiers:
 *  %%   Literal '%'
 *  %Y   Year with century          (e.g. 2026)
 *  %y   Year without century       (e.g. 26)
 *  %m   Month zero-padded          (01..12)
 *  %d   Day of month zero-padded   (01..31)
 *  %H   Hour zero-padded 24h       (00..23)
 *  %M   Minute zero-padded         (00..59)
 *  %S   Second zero-padded         (00..60)
 *  %e   Day of month space-padded  ( 1..31)
 *  %j   Day of year zero-padded    (001..366)
 *  %u   Weekday (1=Mon, 7=Sun)
 *  %w   Weekday (0=Sun, 6=Sat)
 *  %A   Full weekday name          (Monday, ...)
 *  %a   Abbreviated weekday name   (Mon, ...)
 *  %B   Full month name            (January, ...)
 *  %b   Abbreviated month name     (Jan, ...)
 *  %p   AM / PM
 *  %P   am / pm
 *  %I   Hour zero-padded 12h       (01..12)
 *  %n   Newline
 *  %t   Tab
 *  %D   Equivalent to %m/%d/%y
 *  %F   Equivalent to %Y-%m-%d     (ISO 8601 date)
 *  %T   Equivalent to %H:%M:%S     (ISO 8601 time)
 *  %R   Equivalent to %H:%M
 *  %c   Locale date and time       (uses %F %T on VesperaOS)
 *  %x   Locale date                (uses %F on VesperaOS)
 *  %X   Locale time                (uses %T on VesperaOS)
 *
 * @param s      Output buffer.
 * @param max    Size of @p s in bytes.
 * @param fmt    Format string.
 * @param tm     Broken-down time (from gmtime()).
 * @return       Number of bytes written (excluding NUL), or 0 if the buffer
 *               was too small.
 */
size_t strftime(char* s, size_t max, const char* fmt, const struct tm* tm);

/**
 * @brief Format a Unix timestamp directly into a string.
 *
 * Convenience wrapper around gmtime() + strftime().
 *
 * @param s      Output buffer.
 * @param max    Size of @p s.
 * @param fmt    strftime format string.
 * @param t      Unix timestamp.
 * @return       Same as strftime().
 */
size_t strftime_unix(char* s, size_t max, const char* fmt, time_t t);



#endif  // VESPERAOS_TIME_H
