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
#include <bits/alltypes.h>

#define CLOCKS_PER_SEC  ((clock_t)1000000)

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
 * @brief Set CLOCK_REALTIME.
 *
 * @param clk_id  Must be CLOCK_REALTIME.
 * @param ts      New wall-clock time.
 * @return        0 on success, negative errno on failure.
 */
int64_t clock_settime(clockid_t clk_id, const timespec_t* ts);

/**
 * @brief Suspend execution for at least *req time.
 *
 * If interrupted before the full sleep completes and rem != NULL, the
 * remaining time is written to *rem.
 *
 * @param req  Requested sleep duration (relative).
 * @param rem  Remaining time on interruption, or NULL.
 * @return     0 on success, -KEINVAL on bad arguments, -KEINTER if interrupted.
 */
int nanosleep(const timespec_t* req, timespec_t* rem);

/**
 * @brief High-precision sleep against a specific clock.
 *
 * @param clk_id  Clock to sleep against (CLOCK_REALTIME, CLOCK_MONOTONIC, …).
 * @param flags   0 for relative sleep, TIMER_ABSTIME for absolute deadline.
 * @param req     Requested time.
 * @param rem     Remaining time written here if interrupted (relative only).
 * @return        0 on success, negative errno on failure.
 */
int clock_nanosleep(clockid_t clk_id, int flags, const timespec_t* req, timespec_t* rem);

/**
 * @brief Return current Unix time in seconds.
 *
 * @param tloc  If non-NULL, also stored here.
 * @return      Seconds since 1970-01-01 00:00:00 UTC.
 */
time_t time(time_t* tloc);

/**
 * @brief Get time of day (microsecond resolution).
 *
 * tz is ignored (VesperaOS always returns UTC).
 *
 * @param tv   Output: seconds + microseconds since epoch.
 * @param tz   Ignored; pass NULL. has to be implemented
 * @return     0 always.
 */
int gettimeofday(timeval_t* tv, void* tz);

/**
 * @brief Difference between two time_t values in seconds.
 */
double difftime(time_t t1, time_t t0);

/**
 * @brief Return an approximation of CPU time used by the calling process.
 *
 * On VesperaOS this is implemented via CLOCK_PROCESS_CPUTIME_ID when the
 * kernel supports it, or falls back to CLOCK_MONOTONIC uptime otherwise.
 * Divide by CLOCKS_PER_SEC to get seconds:
 *
 *   double elapsed = (double)clock() / CLOCKS_PER_SEC;
 *
 * @return  CPU ticks since process start (unit: 1/CLOCKS_PER_SEC seconds),
 *          or (clock_t)-1 if unavailable.
 */
clock_t clock(void);

/**
 * @brief Decompose a Unix timestamp into a broken-down UTC time.
 *
 * @param timep  Pointer to a time_t value (Unix seconds since epoch).
 * @return       Pointer to a statically allocated struct tm, or NULL on error.
 *               The returned pointer is overwritten on each call.
 */
struct tm* gmtime(const time_t* timep);

/**
 * @brief Decompose a Unix timestamp into local time.
 *
 * VesperaOS has no timezone database; localtime() is identical to gmtime().
 * Both always return UTC.
 *
 * @param timep  Pointer to a time_t value.
 * @return       Pointer to a statically allocated struct tm, or NULL on error.
 */
struct tm* localtime(const time_t* timep);

/**
 * @brief Convert a broken-down UTC time back to a Unix timestamp.
 *
 * Treats the input as UTC (no timezone adjustment).
 * Normalises out-of-range field values, e.g. tm_mon = 13 wraps forward.
 * Updates tm_wday and tm_yday on success.
 *
 * @param tm  Broken-down time to convert (modified in place).
 * @return    Seconds since the Unix epoch, or (time_t)-1 on error.
 */
time_t mktime(struct tm* tm);

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
