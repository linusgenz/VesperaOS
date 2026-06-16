// i8042.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 14.06.26.
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

#include "i8042.h"

#include <vespera/cpu/io.h>

#include <vespera/time.h>

namespace ps2::i8042 {
    [[nodiscard]] bool wait_read(u64 timeout_us) {
        const u64 deadline = kernel::time::get_uptime_us() + timeout_us;
        while (kernel::time::get_uptime_us() < deadline) {
            if (inb(STATUS_PORT) & STATUS_OUTPUT_FULL) return true;
        }
        return false;
    }

    [[nodiscard]] bool wait_write(u64 timeout_us) {
        const u64 deadline = kernel::time::get_uptime_us() + timeout_us;
        while (kernel::time::get_uptime_us() < deadline) {
            if ((inb(STATUS_PORT) & STATUS_INPUT_FULL) == 0) return true;
        }
        return false;
    }

    void drain() {
        for (int i = 0; i < 16; ++i) {
            if ((inb(STATUS_PORT) & STATUS_OUTPUT_FULL) == 0) break;
            inb(DATA_PORT);
        }
    }

    bool aux_send(const u8 byte) {
        for (int retry = 0; retry < 3; ++retry) {
            wait_write();
            outb(CMD_PORT, CMD_WRITE_AUX);
            wait_write();
            outb(DATA_PORT, byte);

            wait_read();
            const u8 resp = inb(DATA_PORT);
            if (resp == RESP_ACK) return true;
            if (resp != RESP_RESEND) break;
        }
        return false;
    }

    bool aux_sliced_cmd(const u8 cmd_byte) {
        constexpr u8 SET_RESOLUTION = 0xE8;

        // Slice 8-bit command byte into 2-bit slices, MSB first
        for (int shift = 6; shift >= 0; shift -= 2) {
            const u8 pair = (cmd_byte >> shift) & 0x03;
            if (!aux_send(SET_RESOLUTION)) return false;
            if (!aux_send(pair)) return false;
        }
        return true;
    }

    MuxInfo probe_hardware_mux() {
        // Probe hardware MUX via loopback on AUX port 3 (command 0xD3)
        constexpr u8 CMD_WRITE_AUX3 = 0xD3;
        constexpr u8 PROBE_BYTE = 0x56;

        drain();

        wait_write();
        outb(CMD_PORT, CMD_WRITE_AUX3);
        wait_write();
        outb(DATA_PORT, PROBE_BYTE);

        wait_read();
        const u8 echo = inb(DATA_PORT);

        drain();

        if (echo == PROBE_BYTE) {
            // MUX exists; retrieve version code using CMD_MUX_VERSION
            wait_write();
            outb(CMD_PORT, 0xA7);
            wait_read();
            const u8 ver = inb(DATA_PORT);

            return MuxInfo{
                .present = true,
                .version = static_cast<u8>(ver & 0x0F),
                .num_ports = 4,
            };
        }

        return MuxInfo{.present = false, .version = 0, .num_ports = 0};
    }
} // namespace ps2::i8042
