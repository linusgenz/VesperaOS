// rtc.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 26.09.25.
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

#ifndef VESPERAOS_RTC_H
#define VESPERAOS_RTC_H

/**
 * @brief Structure representing the current date and time from the RTC device.
 *
 * This structure is returned when reading from `/dev/rtc`.
 * All fields use binary encoding (not BCD).
 */
typedef struct rtc_data {
    u8 sec;   ///< Seconds (0–59)
    u8 min;   ///< Minutes (0–59)
    u8 hour;  ///< Hours (0–23, 24h format)
    u8 day;   ///< Day of the month (1–31)
    u8 month; ///< Month (1–12)
    u8 year;  ///< Year offset from 2000
} rtc_data_t;

#endif //VESPERAOS_RTC_H