// time.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.08.26.
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

#include <sys/time.h>
#include <errno.h>
#include <stdint.h>
#include <sysstd.h>

/**
 * @brief Get current time of day (microsecond resolution).
 */
int gettimeofday(struct timeval *restrict tv, void *restrict tz) {
    if (!tv) {
        errno = EINVAL;
        return -1;
    }

    int res = (int)sys_gettimeofday((uint64_t)tv, (uint64_t)tz, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return 0;
}

/**
 * @brief Set system time (requires privileges).
 */
int settimeofday(const struct timeval *tv, const struct timezone *tz) {
    (void)tv;
    (void)tz;

    errno = ENOSYS;
    return -1;
}

/**
 * @brief Get value of an interval timer.
 */
int getitimer(int which, struct itimerval *curr_value) {
    (void)which;
    if (!curr_value) {
        errno = EINVAL;
        return -1;
    }

    errno = ENOSYS;
    return -1;
}

/**
 * @brief Set value of an interval timer.
 */
int setitimer(int which, const struct itimerval *restrict new_value,
              struct itimerval *restrict old_value) {
    (void)which;
    (void)new_value;
    (void)old_value;

    errno = ENOSYS;
    return -1;
}