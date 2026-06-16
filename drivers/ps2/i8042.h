// i8042.h
//
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

// References:
// - https://wiki.osdev.org/I8042_PS/2_Controller

#ifndef VESPERAOS_DRIVERS_PS2_I8042_H
#define VESPERAOS_DRIVERS_PS2_I8042_H

#include <vespera/types.h>

namespace ps2::i8042 {
    // I/O port addresses
    constexpr u16 DATA_PORT = 0x60;
    constexpr u16 STATUS_PORT = 0x64; // Read: status register
    constexpr u16 CMD_PORT = 0x64; // Write: command register

    // Controller commands (CMD_PORT)
    constexpr u8 CMD_READ_CONFIG = 0x20; // Read controller configuration byte
    constexpr u8 CMD_WRITE_CONFIG = 0x60; // Write controller configuration byte
    constexpr u8 CMD_DISABLE_AUX = 0xA7; // Disable second PS/2 port
    constexpr u8 CMD_ENABLE_AUX = 0xA8; // Enable second PS/2 port
    constexpr u8 CMD_TEST_AUX = 0xA9; // Test second PS/2 port
    constexpr u8 CMD_WRITE_AUX = 0xD4; // Route next DATA_PORT write to AUX device

    // Active hardware multiplexer (i8042 MUX) commands
    constexpr u8 CMD_MUX_VERSION = 0xA7; // MUX query (disables AUX on non-MUX cores)
    constexpr u8 CMD_MUX_PORT_SELECT = 0x90; // 0x90..0x93 selects AUX ports 0..3

    // Status register bits
    constexpr u8 STATUS_OUTPUT_FULL = 0x01; // Output buffer full (data ready to read)
    constexpr u8 STATUS_INPUT_FULL = 0x02; // Input buffer full (busy, do not write)

    // Standard PS/2 responses
    constexpr u8 RESP_ACK = 0xFA;
    constexpr u8 RESP_RESEND = 0xFE;

    struct MuxInfo {
        bool present; // Hardware active MUX detected
        u8 version; // MUX firmware revision nibble
        u8 num_ports; // Supported active ports (usually 4)
    };

    /**
     * Poll until the i8042 has data available in the output buffer.
     */
    bool wait_read(u64 timeout_us = 1000);

    /**
     * Poll until the i8042 input buffer is empty and ready for writes.
     */
    bool wait_write(u64 timeout_us = 1000);

    /**
     * Clear out any stale bytes from the output buffer.
     */
    void drain();

    /**
     * Send a single byte to the AUX port. Retries up to 3 times on RESP_RESEND.
     */
    bool aux_send(u8 byte);

    /**
     * Send an 8-bit Synaptics sliced command query encoded as four 2-bit pairs.
     * Uses SetResolution (0xE8) for encoding.
     */
    [[nodiscard]] bool aux_sliced_cmd(u8 cmd_byte);

    /**
     * Check for a active hardware-level AUX port multiplexer.
     */
    [[nodiscard]] MuxInfo probe_hardware_mux();
} // namespace ps2::i8042

#endif // VESPERAOS_DRIVERS_PS2_I8042_H
