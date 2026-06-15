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
    constexpr u8 SYN_MODE_EWMODE = 0x04;
    constexpr u8 SYN_MODE_WMODE = 0x01; // Report width and pressure

    // Default mode: Absolute + High-Rate + Wmode.
    // EWmode is OR'd in at runtime only if capEWmode is set.
    constexpr u8 SYN_MODE_BASE = SYN_MODE_ABSOLUTE | SYN_MODE_HIGH_RATE | SYN_MODE_WMODE;

    // Synaptics sliced query codes
    constexpr u8 SYN_QUE_IDENTIFY = 0x00;
    constexpr u8 SYN_QUE_CAPABILITIES = 0x02;
    constexpr u8 SYN_QUE_MODEL_ID = 0x03;
    constexpr u8 SYN_QUE_EXTENDED_MODEL = 0x09;

    // Magic constant for Synaptics identification
    constexpr u8 SYN_IDENTITY_MAGIC = 0x47;

    constexpr u8 SYN_W_TWO_FINGERS = 0; // capMultiFinger: two fingers
    constexpr u8 SYN_W_THREE_FINGERS = 1; // capMultiFinger: three or more fingers
    constexpr u8 SYN_W_EW_PACKET = 2; // capEWmode: encapsulated EW packet
    constexpr u8 SYN_W_PASSTHROUGH = 3; // capPassThru: TrackPoint encapsulation

    constexpr u8 SYN_W_FINGER_MIN = 4; // Minimum W for a single finger
    constexpr u8 SYN_W_PALM_THRESHOLD = 10; // Heuristic: W >= this → likely palm

    constexpr u8 SYN_EW_CODE_SCROLL = 0; // Wheel encoder deltas
    constexpr u8 SYN_EW_CODE_SECOND_FINGER = 1; // Secondary finger X/Y/Z
    constexpr u8 SYN_EW_CODE_FINGER_STATE = 2; // Finger count / index

    // Absolute packet synchronization guards
    // For layout see Synaptics PS/2 TouchPad Interfacing Guide page 23
    constexpr u8 SYN_SYNC_MASK = 0xC8; // check bits 7, 6, 3
    constexpr u8 SYN_BYTE0_SYNC = 0x80; // buf[0]: bit7=1, bit6=0, bit3=0
    constexpr u8 SYN_BYTE3_SYNC = 0xC0; // buf[3]: bit7=1, bit6=1, bit3=0

    // Pressure (Z) thresholds
    constexpr u8 SYN_Z_FINGER_DOWN = 25; // Minimum Z to count as finger contact
    constexpr u8 SYN_Z_PALM_MIN = 200; // Z above this → probable palm


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

    struct SYNAPTICS_EXT_MODEL_BYTE0 {
        u8 vertical_scroll : 1; // bit 16
        u8 horizontal_scroll : 1; // bit 17
        u8 ext_w_mode : 1; // bit 18
        u8 vertical_wheel : 1; // bit 19
        u8 glass_pass : 1; // bit 20
        u8 peak_detect : 1; // bit 21
        u8 light_control : 1; // bit 22
        u8 reserved : 1; // bit 23
    } __attribute__((packed));

    struct SYNAPTICS_EXT_MODEL_BYTE1 {
        u8 reserved : 2; // bits 9..8
        u8 info_sensor_ext : 2; // bits 11..10
        u8 n_extended_buttons : 4; // bits 15..12
    } __attribute__((packed));

    union SYNAPTICS_EXT_MODEL_RESPONSE {
        u8 raw[3];

        struct {
            SYNAPTICS_EXT_MODEL_BYTE0 byte0;
            SYNAPTICS_EXT_MODEL_BYTE1 byte1;
            u8 product_id;
        } __attribute__((packed)) fields;
    } __attribute__((packed));


    struct SynapticsInfo {
        SYNAPTICS_IDENTIFY_RESPONSE identity;
        SYNAPTICS_CAPABILITIES_RESPONSE capabilities;
        SYNAPTICS_MODEL_ID_RESPONSE model_id;
        SYNAPTICS_EXT_MODEL_RESPONSE ext_model;
        u8 firmware_major;
        u8 firmware_minor;

        bool has_passthrough;
        bool has_multi_finger;
        bool has_palm_detect;
        bool has_ew_mode;
    };

    /**
     * Probes the PS/2 AUX port for a Synaptics touchpad.
     * Populates out_info on success. Returns true if found.
     */
    [[nodiscard]] bool probe(SynapticsInfo* out_info);

    /**
     * Forwards a single command byte to the TrackPoint via the Synaptics
     * pass-through tunnel (sliced SET_SAMPLE_RATE / 0x28 commit sequence).
     */
    bool tunnel_cmd(u8 cmd);

    /**
     * Switches the touchpad to the specified reporting mode.
     * Uses sliced command sequence committed by sample rate 20.
     */
    [[nodiscard]] bool set_mode(u8 mode_byte);

    /**
     * Caches capability flags from a completed probe() result.
     * Must be called once after set_mode() succeeds so that handle_byte()
     * can dispatch W=0/1/2/3 packets correctly without re-querying hardware.
     */
    void set_caps(const SynapticsInfo& info);

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
