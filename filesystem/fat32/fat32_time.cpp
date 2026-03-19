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

#include <vespera/time.h>

#include "fat32.h"

namespace fat32 {

    struct FsTime {
        u16 date;
        u16 time;
        u8 tenths;
    };

    static u16 encode_fat_date(const u8 day, const u8 month, u16 year) {
        if (year < 1980) year = 1980;
        if (year > 2107) year = 2107;

        return static_cast<u16>(((year - 1980) << 9) | (month << 5) | day);
    }

    static u16 encode_fat_time(const u8 hour, const u8 minute, const u8 second) {
        return static_cast<u16>((hour << 11) | (minute << 5) | (second / 2));
    }

    static FsTime get_current_fat_time() {
        u8 sec = 0, min = 0, hour = 0, day = 0, month = 0, year = 0;

        kernel::time::read_rtc(sec, min, hour, day, month, year);

        // RTC just delivers year since 2000 so we have to add 2000 here
        const u16 full_year = 2000 + year;

        FsTime t{};
        t.date = encode_fat_date(day, month, full_year);
        t.time = encode_fat_time(hour, min, sec);
        t.tenths = static_cast<u8>((sec % 2) * 100);  // FAT: 0–199

        return t;
    }

    void update_create_time(DirectoryEntry& e) {
        auto [date, time, tenths] = get_current_fat_time();
        e.creation_date = date;
        e.creation_time = time;
        e.creation_time_tenths = tenths;
        e.last_access_date = date;
        e.write_date = date;
        e.write_time = time;
    }

    void update_write_time(DirectoryEntry& e) {
        const FsTime t = get_current_fat_time();
        e.write_date = t.date;
        e.write_time = t.time;
        e.last_access_date = t.date;
    }

    void update_access_time(DirectoryEntry& e) {
        const FsTime t = get_current_fat_time();
        e.last_access_date = t.date;
    }
}  // namespace fat32
