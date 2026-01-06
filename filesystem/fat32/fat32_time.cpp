/**
 * @file fat32_time.cpp
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 05.01.26.
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

#include <cstdint>
#include <kernel/time.h>
#include "fat32.h"

namespace FAT32
{

    struct FsTime
    {
        uint16_t date;
        uint16_t time;
        uint8_t tenths;
    };

    static uint16_t EncodeFatDate(uint8_t day, uint8_t month, uint16_t year)
    {
        if (year < 1980) year = 1980;
        if (year > 2107) year = 2107;

        return static_cast<uint16_t>(
            ((year - 1980) << 9) |
            (month << 5) |
            day
        );
    }

    static uint16_t EncodeFatTime(uint8_t hour, uint8_t minute, uint8_t second)
    {
        return static_cast<uint16_t>(
            (hour << 11) |
            (minute << 5) |
            (second / 2)
        );
    }

    static FsTime GetCurrentFatTime()
    {
        uint8_t sec, min, hour, day, month, year;

        kernel::time::read_rtc(sec, min, hour, day, month, year);

        // RTC just delivers year since 2000 so we have to add 2000 here
        uint16_t fullYear = 2000 + year;

        FsTime t{};
        t.date = EncodeFatDate(day, month, fullYear);
        t.time = EncodeFatTime(hour, min, sec);
        t.tenths = static_cast<uint8_t>((sec % 2) * 100); // FAT: 0–199

        return t;
    }

    void UpdateCreateTime(DirectoryEntry& e)
    {
        FsTime t = GetCurrentFatTime();
        e.creationDate = t.date;
        e.creationTime = t.time;
        e.creationTimeTenths = t.tenths;
        e.lastAccessDate = t.date;
        e.writeDate = t.date;
        e.writeTime = t.time;
    }

    void UpdateWriteTime(DirectoryEntry& e)
    {
        FsTime t = GetCurrentFatTime();
        e.writeDate = t.date;
        e.writeTime = t.time;
        e.lastAccessDate = t.date;
    }

    void UpdateAccessTime(DirectoryEntry& e)
    {
        FsTime t = GetCurrentFatTime();
        e.lastAccessDate = t.date;
    }
}
