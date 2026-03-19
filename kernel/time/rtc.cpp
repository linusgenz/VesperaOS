//
// Created by Linus on 12.07.25.
//

#include <vespera/time.h>

#include <vespera/log.h>

namespace kernel::time {
    u8 cmos_read(u8 reg) {
        asm volatile("outb %0, %1" : : "a"(reg), "Nd"(CMOS_ADDRESS));
        u8 value;
        asm volatile("inb %1, %0" : "=a"(value) : "Nd"(CMOS_DATA));
        return value;
    }

    u8 bcd_to_binary(const u8 bcd) {
        return ((bcd / 16) * 10) + (bcd & 0x0F);
    }

    void read_rtc(u8& second, u8& minute, u8& hour, u8& day, u8& month, u8& year) {
        while (cmos_read(0x0A) & 0x80) {
        }

        second = cmos_read(0x00);
        minute = cmos_read(0x02);
        hour = cmos_read(0x04);
        day = cmos_read(0x07);
        month = cmos_read(0x08);
        year = cmos_read(0x09);

        const u8 status_b = cmos_read(0x0B);

        if (!(status_b & 0x04)) {
            second = bcd_to_binary(second);
            minute = bcd_to_binary(minute);
            hour = bcd_to_binary(hour);
            day = bcd_to_binary(day);
            month = bcd_to_binary(month);
            year = bcd_to_binary(year);
        }

        // 1 = 24h, 0 = 12h
        if (!(status_b & 0x02) && (hour & 0x80)) {
            hour = ((hour & 0x7F) + 12) % 24;
        }
    }

    void print_current_time() {
        u8 sec, min, hour, day, month, year;
        read_rtc(sec, min, hour, day, month, year);

        Log::info("Time: %u:%u:%u Date: %u.%u.%u", hour, min, sec, day, month, year);
    }

}  // namespace kernel::time