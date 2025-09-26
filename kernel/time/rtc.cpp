//
// Created by Linus on 12.07.25.
//


#include "../include/time.h"
#include "../../include/log.h"

namespace kernel::time {
    uint8_t cmos_read(uint8_t reg) {
        asm volatile ("outb %0, %1" : : "a"(reg), "Nd"(CMOS_ADDRESS));
        uint8_t value;
        asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(CMOS_DATA));
        return value;
    }

    uint8_t bcd_to_binary(uint8_t bcd) {
        return ((bcd / 16) * 10) + (bcd & 0x0F);
    }

    void read_rtc(uint8_t& second, uint8_t& minute, uint8_t& hour,
                  uint8_t& day, uint8_t& month, uint8_t& year) {

        while (cmos_read(0x0A) & 0x80) {}

        second = cmos_read(0x00);
        minute = cmos_read(0x02);
        hour   = cmos_read(0x04);
        day    = cmos_read(0x07);
        month  = cmos_read(0x08);
        year   = cmos_read(0x09);

        uint8_t status_b = cmos_read(0x0B);

        if (!(status_b & 0x04)) {
            second = bcd_to_binary(second);
            minute = bcd_to_binary(minute);
            hour   = bcd_to_binary(hour);
            day    = bcd_to_binary(day);
            month  = bcd_to_binary(month);
            year   = bcd_to_binary(year);
        }

        // 1 = 24h, 0 = 12h
        if (!(status_b & 0x02) && (hour & 0x80)) {
            hour = ((hour & 0x7F) + 12) % 24;
        }
    }

    void print_current_time() {
        uint8_t sec, min, hour, day, month, year;
        read_rtc(sec, min, hour, day, month, year);

        Log::Info("Time: %u:%u:%u Date: %u.%u.%u",
                  hour, min, sec, day, month, year);
    }

}