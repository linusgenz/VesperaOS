// ps2_defs.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <mail@linusgenz.dev>
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

// Shared PS/2 protocol constants used across the mouse, Synaptics, and i8042
// drivers.  Nothing in here has side effects – include freely.

#ifndef VESPERAOS_DRIVERS_PS2_PS2_DEFS_H
#define VESPERAOS_DRIVERS_PS2_PS2_DEFS_H

#include <vespera/types.h>

namespace ps2 {

    // I/O ports
    constexpr u16 DATA_PORT = 0x60;
    constexpr u16 CMD_PORT  = 0x64;

    // Controller commands (written to CMD_PORT)
    constexpr u8 CMD_ENABLE_AUX_PORT    = 0xA8;
    constexpr u8 CMD_READ_CONFIG_BYTE   = 0x20;
    constexpr u8 CMD_WRITE_CONFIG_BYTE  = 0x60;

    // Generic mouse / AUX device commands
    constexpr u8 CMD_DISABLE            = 0xF5; // Disable data reporting
    constexpr u8 CMD_ENABLE             = 0xF4; // Enable data reporting
    constexpr u8 CMD_SET_DEFAULTS       = 0xF6;
    constexpr u8 CMD_GET_DEVICE_ID      = 0xF2;
    constexpr u8 CMD_SET_SAMPLE_RATE    = 0xF3;
    constexpr u8 CMD_STATUS_REQUEST     = 0xE9; // Used in Synaptics sliced queries

    // Sample rates
    constexpr u8 SAMPLE_RATE_200 = 200;
    constexpr u8 SAMPLE_RATE_100 = 100;
    constexpr u8 SAMPLE_RATE_80  = 80;
    constexpr u8 SAMPLE_RATE_20  = 20;  // Synaptics mode-commit sentinel

    //Standard mouse packet flags (byte 0)
    constexpr u8 PKT_LEFT_BUTTON  = 0b00000001;
    constexpr u8 PKT_RIGHT_BUTTON = 0b00000010;
    constexpr u8 PKT_MIDDLE_BUTTON= 0b00000100;
    constexpr u8 PKT_ALWAYS_ONE   = 0b00001000;
    constexpr u8 PKT_X_SIGN       = 0b00010000;
    constexpr u8 PKT_Y_SIGN       = 0b00100000;
    constexpr u8 PKT_X_OVERFLOW   = 0b01000000;
    constexpr u8 PKT_Y_OVERFLOW   = 0b10000000;

    // Device IDs returned by CMD_GET_DEVICE_ID
    constexpr u8 DEVICE_ID_STANDARD    = 0x00;
    constexpr u8 DEVICE_ID_INTELLIMOUSE = 0x03;

    // Synaptics-specific commands / magic values
    // Commit byte for the Synaptics set-mode sliced-command sequence.
    // Writing SET_SAMPLE_RATE followed by this byte latches the mode register.
    constexpr u8 SYN_SAMPLE_RATE_MODE_COMMIT   = 0x14;

    // Commit byte for the Synaptics pass-through tunnel sequence.
    // Writing SET_SAMPLE_RATE followed by this byte forwards the buffered
    // sliced command to the TrackPoint.
    constexpr u8 SYN_SAMPLE_RATE_TUNNEL_COMMIT = 0x28;

} // namespace ps2

#endif // VESPERAOS_DRIVERS_PS2_PS2_DEFS_H