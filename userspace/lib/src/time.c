// time.c
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

#include <stdio.h>
#include <string.h>
#include <time.h>

int64_t clock_gettime(clockid_t clk_id, timespec_t* ts) {
    return sys_clock_gettime((uint64_t)clk_id, (uint64_t)ts, 0, 0, 0, 0);
}

int64_t clock_settime(clockid_t clk_id, const timespec_t* ts) {
    return -1;
}

int nanosleep(const timespec_t* req, timespec_t* rem) {
    return (int)sys_nanosleep((uint64_t)req, (uint64_t)rem, 0, 0, 0, 0);
}

int clock_nanosleep(clockid_t clk_id, int flags, const timespec_t* req, timespec_t* rem) {
    return (int)sys_clock_nanosleep((uint64_t)clk_id, (uint64_t)flags, (uint64_t)req, (uint64_t)rem, 0, 0);
}

time_t time(time_t* tloc) {
    return (time_t)sys_time((uint64_t)tloc, 0, 0, 0, 0, 0);
}

int gettimeofday(timeval_t* tv, void* tz) {
    return (int)sys_gettimeofday((uint64_t)tv, (uint64_t)tz, 0, 0, 0, 0);
}

double difftime(time_t t1, time_t t0) {
    return (double)(t1 - t0);
}

clock_t clock(void) {
    timespec_t ts;

    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0) {
        return (clock_t)(ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL);
    }

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (clock_t)(ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL);
    }

    return (clock_t)-1;
}

static const int DAYS_PER_MONTH[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static int is_leap(int year) {
    return (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
}

static int days_in_month(int mon, int year) {
    if (mon == 1 && is_leap(year)) return 29;
    return DAYS_PER_MONTH[mon];
}

struct tm* gmtime_r(const time_t* __restrict__ timep, struct tm* __restrict__ result) {
    if (!timep || !result) return NULL;

    uint64_t t = (uint64_t)*timep;

    result->tm_sec = (int)(t % 60);
    t /= 60;
    result->tm_min = (int)(t % 60);
    t /= 60;
    result->tm_hour = (int)(t % 24);
    t /= 24;

    // t is now days since 1970-01-01
    // Calculate weekday (1970-01-01 was a Thursday = 4)
    result->tm_wday = (int)((t + 4) % 7);

    // Walk through years
    int year = 1970;
    while (1) {
        int days_this_year = is_leap(year) ? 366 : 365;
        if (t < (uint64_t)days_this_year) break;
        t -= days_this_year;
        year++;
    }

    result->tm_year = year - 1900;
    result->tm_yday = (int)t;

    // Walk through months
    int mon = 0;
    while (mon < 12) {
        int dim = days_in_month(mon, year);
        if (t < (uint64_t)dim) break;
        t -= dim;
        mon++;
    }

    result->tm_mon = mon;
    result->tm_mday = (int)t + 1;
    result->tm_isdst = 0;

    return result;
}

struct tm* localtime_r(const time_t* __restrict__ timer, struct tm* __restrict__ result) {
    return gmtime_r(timer, result);
}

struct tm* gmtime(const time_t* timep) {
    static struct tm result;
    return gmtime_r(timep, &result);
}

struct tm* localtime(const time_t* timep) {
    static struct tm result;
    return localtime_r(timep, &result);
}

time_t mktime(struct tm* tm) {
    if (!tm) return (time_t)-1;

    tm->tm_min += tm->tm_sec / 60;
    tm->tm_sec %= 60;
    tm->tm_hour += tm->tm_min / 60;
    tm->tm_min %= 60;
    tm->tm_mday += tm->tm_hour / 24;
    tm->tm_hour %= 24;

    // Fix negative remainders
    if (tm->tm_sec < 0) {
        tm->tm_min--;
        tm->tm_sec += 60;
    }
    if (tm->tm_min < 0) {
        tm->tm_hour--;
        tm->tm_min += 60;
    }
    if (tm->tm_hour < 0) {
        tm->tm_mday--;
        tm->tm_hour += 24;
    }

    tm->tm_year += tm->tm_mon / 12;
    tm->tm_mon %= 12;
    if (tm->tm_mon < 0) {
        tm->tm_year--;
        tm->tm_mon += 12;
    }

    // Walk backwards when tm_mday < 1.
    while (tm->tm_mday < 1) {
        tm->tm_mon--;
        if (tm->tm_mon < 0) {
            tm->tm_year--;
            tm->tm_mon = 11;
        }
        tm->tm_mday += days_in_month(tm->tm_mon, tm->tm_year + 1900);
    }
    // Walk forwards when tm_mday > days in the current month.
    for (;;) {
        int dim = days_in_month(tm->tm_mon, tm->tm_year + 1900);
        if (tm->tm_mday <= dim) break;
        tm->tm_mday -= dim;
        tm->tm_mon++;
        if (tm->tm_mon > 11) {
            tm->tm_year++;
            tm->tm_mon = 0;
        }
    }

    int full_year = tm->tm_year + 1900;
    if (full_year < 1970) return (time_t)-1;

    uint64_t days = 0;
    for (int y = 1970; y < full_year; y++) days += (uint64_t)(is_leap(y) ? 366 : 365);

    tm->tm_yday = 0;
    for (int m = 0; m < tm->tm_mon; m++) {
        int d = days_in_month(m, full_year);
        days += (uint64_t)d;
        tm->tm_yday += d;
    }
    days += (uint64_t)(tm->tm_mday - 1);
    tm->tm_yday += tm->tm_mday - 1;

    tm->tm_wday = (int)((days + 4) % 7);
    tm->tm_isdst = 0;

    return (time_t)(days * 86400ULL + (uint64_t)tm->tm_hour * 3600ULL + (uint64_t)tm->tm_min * 60ULL +
                    (uint64_t)tm->tm_sec);
}

static const char* const weekday_full[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

static const char* const weekday_abbr[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static const char* const month_full[12] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

static const char* const month_abbr[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static int append_str(char* buf, size_t* pos, size_t max, const char* s) {
    while (*s) {
        if (*pos >= max - 1) return 0;
        buf[(*pos)++] = *s++;
    }
    return 1;
}

static int append_int(char* buf, size_t* pos, size_t max, int val, int width, char pad) {
    char tmp[16];
    // Build the number string right-to-left.
    int i = sizeof(tmp) - 1;
    tmp[i] = '\0';
    if (val == 0) {
        tmp[--i] = '0';
    } else {
        int v = val < 0 ? -val : val;
        while (v > 0) {
            tmp[--i] = '0' + v % 10;
            v /= 10;
        }
        if (val < 0) tmp[--i] = '-';
    }
    // Pad on the left.
    int len = (int)(sizeof(tmp) - 1 - i);
    while (len < width) {
        tmp[--i] = pad;
        len++;
    }

    return append_str(buf, pos, max, &tmp[i]);
}

static size_t strftime_impl(char* s, size_t max, const char* fmt, const struct tm* tm) {
    size_t pos = 0;

    while (*fmt) {
        if (*fmt != '%') {
            if (pos >= max - 1) return 0;
            s[pos++] = *fmt++;
            continue;
        }

        fmt++;  // skip '%'
        char spec = *fmt++;

        switch (spec) {
            case '%':
                if (pos >= max - 1) return 0;
                s[pos++] = '%';
                break;

            case 'Y':
                if (!append_int(s, &pos, max, tm->tm_year + 1900, 4, '0')) return 0;
                break;
            case 'y':
                if (!append_int(s, &pos, max, (tm->tm_year + 1900) % 100, 2, '0')) return 0;
                break;
            case 'm':
                if (!append_int(s, &pos, max, tm->tm_mon + 1, 2, '0')) return 0;
                break;
            case 'd':
                if (!append_int(s, &pos, max, tm->tm_mday, 2, '0')) return 0;
                break;
            case 'e':
                if (!append_int(s, &pos, max, tm->tm_mday, 2, ' ')) return 0;
                break;
            case 'H':
                if (!append_int(s, &pos, max, tm->tm_hour, 2, '0')) return 0;
                break;
            case 'I': {
                int h12 = tm->tm_hour % 12;
                if (h12 == 0) h12 = 12;
                if (!append_int(s, &pos, max, h12, 2, '0')) return 0;
                break;
            }
            case 'M':
                if (!append_int(s, &pos, max, tm->tm_min, 2, '0')) return 0;
                break;
            case 'S':
                if (!append_int(s, &pos, max, tm->tm_sec, 2, '0')) return 0;
                break;
            case 'j':
                if (!append_int(s, &pos, max, tm->tm_yday + 1, 3, '0')) return 0;
                break;
            case 'u': {
                int u = tm->tm_wday == 0 ? 7 : tm->tm_wday;
                if (!append_int(s, &pos, max, u, 1, '0')) return 0;
                break;
            }
            case 'w':
                if (!append_int(s, &pos, max, tm->tm_wday, 1, '0')) return 0;
                break;

            case 'A':
                if (tm->tm_wday < 0 || tm->tm_wday > 6) return 0;
                if (!append_str(s, &pos, max, weekday_full[tm->tm_wday])) return 0;
                break;
            case 'a':
                if (tm->tm_wday < 0 || tm->tm_wday > 6) return 0;
                if (!append_str(s, &pos, max, weekday_abbr[tm->tm_wday])) return 0;
                break;
            case 'B':
                if (tm->tm_mon < 0 || tm->tm_mon > 11) return 0;
                if (!append_str(s, &pos, max, month_full[tm->tm_mon])) return 0;
                break;
            case 'b':
            case 'h':
                if (tm->tm_mon < 0 || tm->tm_mon > 11) return 0;
                if (!append_str(s, &pos, max, month_abbr[tm->tm_mon])) return 0;
                break;

            case 'p':
                if (!append_str(s, &pos, max, tm->tm_hour < 12 ? "AM" : "PM")) return 0;
                break;
            case 'P':
                if (!append_str(s, &pos, max, tm->tm_hour < 12 ? "am" : "pm")) return 0;
                break;

            case 'n':
                if (!append_str(s, &pos, max, "\n")) return 0;
                break;
            case 't':
                if (!append_str(s, &pos, max, "\t")) return 0;
                break;

            case 'F': {  // %Y-%m-%d
                size_t n = strftime_impl(s + pos, max - pos, "%Y-%m-%d", tm);
                if (n == 0 && max - pos > 1) return 0;
                pos += n;
                break;
            }
            case 'T': {  // %H:%M:%S
                size_t n = strftime_impl(s + pos, max - pos, "%H:%M:%S", tm);
                if (n == 0 && max - pos > 1) return 0;
                pos += n;
                break;
            }
            case 'R': {  // %H:%M
                size_t n = strftime_impl(s + pos, max - pos, "%H:%M", tm);
                if (n == 0 && max - pos > 1) return 0;
                pos += n;
                break;
            }
            case 'D': {  // %m/%d/%y
                size_t n = strftime_impl(s + pos, max - pos, "%m/%d/%y", tm);
                if (n == 0 && max - pos > 1) return 0;
                pos += n;
                break;
            }
            case 'c': {  // locale date+time
                size_t n = strftime_impl(s + pos, max - pos, "%F %T", tm);
                if (n == 0 && max - pos > 1) return 0;
                pos += n;
                break;
            }
            case 'x': {  // locale date
                size_t n = strftime_impl(s + pos, max - pos, "%F", tm);
                if (n == 0 && max - pos > 1) return 0;
                pos += n;
                break;
            }
            case 'X': {  // locale time
                size_t n = strftime_impl(s + pos, max - pos, "%T", tm);
                if (n == 0 && max - pos > 1) return 0;
                pos += n;
                break;
            }

            default:
                if (pos >= max - 1) return 0;
                s[pos++] = '%';
                if (pos >= max - 1) return 0;
                s[pos++] = spec;
                break;
        }
    }

    s[pos] = '\0';
    return pos;
}

size_t strftime(char* s, size_t max, const char* fmt, const struct tm* tm) {
    if (!s || max == 0 || !fmt || !tm) return 0;
    return strftime_impl(s, max, fmt, tm);
}

size_t strftime_unix(char* s, size_t max, const char* fmt, time_t t) {
    struct tm* tm = gmtime(&t);
    if (!tm) return 0;
    return strftime(s, max, fmt, tm);
}