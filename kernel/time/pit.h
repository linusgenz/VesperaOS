// pit.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.04.26.
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

#ifndef VESPERAOS_KERNEL_TIME_PIT_H
#define VESPERAOS_KERNEL_TIME_PIT_H

#include <vespera/types.h>
#include "clock_source.h"

namespace kernel::time {

    constexpr u16 PIT_PORT_CHANNEL0 = 0x40; // counter 0 data port (read/write)
    constexpr u16 PIT_PORT_CHANNEL2 = 0x42; // counter 2 data port (PC speaker, one-shot)
    constexpr u16 PIT_PORT_COMMAND  = 0x43; // mode/command register (write-only)
    constexpr u16 PIT_PORT_GATE     = 0x61; // system control port B (gate + output for ch2)

    // Mode/command byte fields  (Intel 8254 §7)
    constexpr u8 PIT_CMD_CHANNEL0   = 0x00; // select channel 0
    constexpr u8 PIT_CMD_CHANNEL2   = 0x80; // select channel 2
    constexpr u8 PIT_CMD_ACCESS_LO  = 0x10; // access mode: low byte only
    constexpr u8 PIT_CMD_ACCESS_HI  = 0x20; // access mode: high byte only
    constexpr u8 PIT_CMD_ACCESS_LOH = 0x30; // access mode: low byte then high byte
    constexpr u8 PIT_CMD_ACCESS_LATCH = 0x00; // counter latch (read snapshot)
    constexpr u8 PIT_CMD_MODE0      = 0x00; // mode 0: interrupt on terminal count
    constexpr u8 PIT_CMD_MODE2      = 0x04; // mode 2: rate generator
    constexpr u8 PIT_CMD_MODE3      = 0x06; // mode 3: square-wave generator
    constexpr u8 PIT_CMD_BINARY     = 0x00; // binary counting

    // The PIT oscillator is derived from the original IBM AT clock at exactly:
    constexpr u64 PIT_BASE_FREQ_HZ = 1'193'182; // Hz

    // Interrupt rate for the clock source tick (channel 0, mode 2).
    constexpr u64 PIT_TICK_HZ     = 1'000;       // 1 ms resolution
    constexpr u16 PIT_DIVISOR     = static_cast<u16>(PIT_BASE_FREQ_HZ / PIT_TICK_HZ);

    class PitClock final : public IClockSource {
    public:
        PitClock()  = default;
        ~PitClock() override = default;

        PitClock(const PitClock&)            = delete;
        PitClock& operator=(const PitClock&) = delete;

        [[nodiscard]] const char*     name()         const override { return "PIT"; }
        [[nodiscard]] int             init()                override;
        [[nodiscard]] bool            available()    const override { return available_; }
        [[nodiscard]] clock_priority  priority()     const override { return clock_priority::PIT; }
        [[nodiscard]] u64             read_ticks()         override;
        [[nodiscard]] u64             frequency_hz() const override { return PIT_TICK_HZ; }
        [[nodiscard]] u64             read_ns()            override;

        void on_irq();

        // Maximum delay: ~54 ms (single 16-bit counter rollover).
        static void busy_wait_us(u32 us);

    private:
        [[nodiscard]] static u16 latch_read();

        volatile u64 irq_ticks_  = 0; // incremented every PIT_TICK_HZ interrupt
        bool         available_  = false;
    };

} // namespace kernel::time

#endif // VESPERAOS_KERNEL_TIME_PIT_H
