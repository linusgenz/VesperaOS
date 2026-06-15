// synaptics_ps2.h
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

// The TrackPoint (pointing stick) is NOT a separate PS/2 AUX device. It is
// internally wired to the Synaptics pad's "pass-through" port. The pad acts as
// a software multiplexer:
//
//   ┌─────────────────────────────────────────────┐
//   │  i8042 / PCH                                │
//   │   IRQ1  → keyboard                          │
//   │   IRQ12 → Synaptics PS/2 pad → TrackPoint   │
//   └─────────────────────────────────────────────┘
//
// In the default PS/2 compatibility mode the pad sends standard 3-byte relative
// packets that describe touchpad movement only; the TrackPoint is invisible.
//
// References:
//  - Synaptics PS/2 TouchPad Interfacing Guide:
//    http://blog.amigas.ru/wp-content/uploads/2014/03/touchpad_RevB.pdf
//  - https://wiki.osdev.org/PS/2_Mouse (general PS/2 protocol)

#ifndef VESPERAOS_DRIVERS_PS2_SYNAPTICS_SYNAPTICS_PS2_H
#define VESPERAOS_DRIVERS_PS2_SYNAPTICS_SYNAPTICS_PS2_H

#include <vespera/types.h>

namespace ps2::synaptics {
    // Synaptics mode bits
    constexpr u8 SYN_MODE_ABSOLUTE = 0x80; // Absolute coordinates reporting
    constexpr u8 SYN_MODE_HIGH_RATE = 0x40; // 80 reports/s rate
    constexpr u8 SYN_MODE_SLEEP = 0x08; // Low-power sleep
    constexpr u8 SYN_MODE_WMODE = 0x01; // Report width and pressure

    // Default mode for TrackPoint passthrough + absolute touchpad data
    constexpr u8 SYN_MODE_DEFAULT = SYN_MODE_ABSOLUTE | SYN_MODE_HIGH_RATE | SYN_MODE_WMODE;

    // Synaptics sliced query codes
    constexpr u8 SYN_QUE_IDENTIFY = 0x00;
    constexpr u8 SYN_QUE_CAPABILITIES = 0x02;
    constexpr u8 SYN_QUE_MODEL_ID = 0x03;

    // Magic constant for Synaptics identification
    constexpr u8 SYN_IDENTITY_MAGIC = 0x47;

    constexpr u32 SYN_CAP_PASS_THROUGH = (1u << 19); // TrackPoint presence bit

    // Passthrough packet signatures (TrackPoint)
    constexpr u8 SYN_PT_BYTE0_MASK = 0xFC;
    constexpr u8 SYN_PT_BYTE0_VAL = 0x84;
    constexpr u8 SYN_PT_BYTE3_MASK = 0xCC;
    constexpr u8 SYN_PT_BYTE3_VAL = 0xC4;

    // Absolute packet synchronization guards
    // For layout see Synaptics PS/2 TouchPad Interfacing Guide page 23
    constexpr u8 SYN_SYNC_MASK = 0xC8; // check bits 7, 6, 3
    constexpr u8 SYN_BYTE0_SYNC = 0x80; // buf[0]: bit7=1, bit6=0, bit3=0
    constexpr u8 SYN_BYTE3_SYNC = 0xC0; // buf[3]: bit7=1, bit6=1, bit3=0

    enum class SYNAPTICS_GEOMETRY : u8 {
        UNKNOWN = 0,
        RECTANGLE = 1,
        ROUND = 2,
        ROUNDED_RECTANGLE = 3,
        RACE_TRACK = 4
    };

    struct SYNAPTICS_IDENTIFY_FIELDS {
        u8 info_minor;
        u8 magic; // Byte 2 (sollte 0x47 sein)
        u8 info_major : 4; // Bits 0-3
        u8 info_model_code : 4; // Bits 4-7
    } __attribute__((packed));

    union SYNAPTICS_IDENTIFY_RESPONSE {
        u8 raw[3];
        SYNAPTICS_IDENTIFY_FIELDS fields;
    } __attribute__((packed));

    struct SYNAPTICS_CAPABILITIES_BYTE1 {
        u8 reserved1 : 2; // bits 1:0  (Reserved)
        u8 cap_middle_button : 1; // bit 2     (capMiddleButton)
        u8 reserved2 : 1; // bit 3     (Reserved)
        u8 n_extended_queries : 3; // bits 6:4  (nExtendedQueries)
        u8 cap_extended : 1; // bit 7     (capExtended)
    } __attribute__((packed));

    struct SYNAPTICS_CAPABILITIES_BYTE3 {
        u8 cap_palm_detect : 1; // Bit 0
        u8 cap_multi_finger : 1; // Bit 1
        u8 cap_ballistics : 1; // Bit 2
        u8 cap_four_buttons : 1; // Bit 3
        u8 cap_sleep : 1; // Bit 4
        u8 cap_multi_finger_report : 1; // Bit 5
        u8 cap_low_power : 1; // Bit 6
        u8 cap_pass_through : 1; // Bit 7
    } __attribute__((packed));

    // Layout for FW >= 7.5 (Fig 4-7)
    struct SYNAPTICS_CAPABILITIES_NEW {
        SYNAPTICS_CAPABILITIES_BYTE1 byte1;
        u8 model_sub_number;
        SYNAPTICS_CAPABILITIES_BYTE3 byte3;
    } __attribute__((packed));

    // Layout for FW < 7.5 (Fig 4-8)
    struct SYNAPTICS_CAPABILITIES_OLD {
        u8 reserved1 : 1;
        u8 n_extended_queries : 3;
        u8 cap_extended : 1;
        u8 cap_middle_button : 1;
        u8 reserved2 : 2;
        u8 magic; // Fixer Wert 0x47
        SYNAPTICS_CAPABILITIES_BYTE3 byte3;
    } __attribute__((packed));

    union SYNAPTICS_CAPABILITIES_RESPONSE {
        u8 raw[3];
        SYNAPTICS_CAPABILITIES_NEW c_new;
        SYNAPTICS_CAPABILITIES_OLD c_old;
    } __attribute__((packed));

    struct SYNAPTICS_MODEL_ID_FIELDS {
        // Byte 1
        u8 info_sensor : 6; // Bits 16-21
        u8 info_portrait : 1; // Bit 22
        u8 info_rot180 : 1; // Bit 23

        // Byte 2
        u8 reserved3 : 1; // Bit 8
        u8 info_hardware : 7; // Bits 9-15

        // Byte 3
        SYNAPTICS_GEOMETRY info_geometry : 4; // Bits 0-3
        u8 reserved1 : 1; // Bit 4
        u8 info_simple_cmd : 1; // Bit 5
        u8 reserved2 : 1; // Bit 6
        u8 info_new_abs : 1; // Bit 7
    } __attribute__((packed));

    union SYNAPTICS_MODEL_ID_RESPONSE {
        u8 raw[3];
        SYNAPTICS_MODEL_ID_FIELDS fields;
    } __attribute__((packed));

    struct SynapticsInfo {
        SYNAPTICS_IDENTIFY_RESPONSE identity;
        SYNAPTICS_CAPABILITIES_RESPONSE capabilities;
        SYNAPTICS_MODEL_ID_RESPONSE model_id;
        u8 firmware_major;
        u8 firmware_minor;
        bool has_passthrough;
    };

    /**
     * Probes the PS/2 AUX port for a Synaptics touchpad.
     * Populates out_info on success. Returns true if found.
     */
    [[nodiscard]] bool probe(SynapticsInfo* out_info);

    bool tunnel_cmd(u8 cmd);

    /**
     * Switches the touchpad to the specified reporting mode.
     * Uses sliced command sequence committed by sample rate 20.
     */
    [[nodiscard]] bool set_mode(u8 mode_byte);

    /**
     * Returns true if the Synaptics driver is active.
     */
    bool is_active();

    /**
     * Handles an incoming byte from the AUX port.
     * Parses absolute packets and extracts TrackPoint events.
     */
    void handle_byte(u8 data);
} // namespace ps2::synaptics

#endif // VESPERAOS_DRIVERS_PS2_SYNAPTICS_SYNAPTICS_PS2_H
