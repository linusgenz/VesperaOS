/**
 * @file uptime.c
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 06.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdint.h>

#include "../lib/include/fflags.h"
#include "../lib/include/stdio.h"

#define UPTIME_DEVICE "/dev/uptime"

int main(int argc, char** argv)
{
    HANDLE hdl = open("/dev/uptime", O_RDONLY);
    if (hdl < 0)
    {
        printf("uptime: cannot open /dev/uptime (hdl=%lld)\n", (long long)hdl);
        return -1;
    }

    uint64_t ms;
    ssize_t r = read(hdl, &ms, sizeof(ms));
    if (r != sizeof(ms))
    {
        printf("uptime: cannot read uptime\n");
        return -1;
    }

    uint64_t seconds = ms / 1000;
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;

    seconds %= 60;
    minutes %= 60;

    printf("up %lu hours, %lu minutes, %lu seconds\n",
           hours, minutes, seconds);
    return 0;
}
