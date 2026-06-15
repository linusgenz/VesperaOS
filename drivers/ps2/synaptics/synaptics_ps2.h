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
    constexpr u8 SYN_QUE_CONTINUED_CAPS = 0x0C;
    constexpr u8 SYN_QUE_MAX_COORDS = 0x0D;
    constexpr u8 SYN_QUE_MIN_COORDS = 0x0F;

    // Magic constant for Synaptics identification
    constexpr u8 SYN_IDENTITY_MAGIC = 0x47;

    constexpr u8 SYN_W_TWO_FINGERS = 0; // capMultiFinger: two fingers
    constexpr u8 SYN_W_THREE_FINGERS = 1; // capMultiFinger: three or more fingers
    constexpr u8 SYN_W_EW_PACKET = 2; // capEWmode: encapsulated EW packet
    constexpr u8 SYN_W_PASSTHROUGH = 3; // capPassThru: TrackPoint encapsulation

    constexpr u8 SYN_W_FINGER_MIN = 4; // Minimum W for a single finger
    constexpr u8 SYN_W_PALM_THRESHOLD = 10; // Heuristic: W >= this → likely palm

    // ============================================================================
    // EXTENDED W PACKETS
    // ============================================================================

    enum SYNAPTICS_EW_PACKET_TYPE : u8 {
        SYNAPTICS_EW_PACKET_SCROLL = 0,
        SYNAPTICS_EW_PACKET_SECOND_FINGER = 1,
        SYNAPTICS_EW_PACKET_FINGER_STATE = 2,
    };

    // Absolute packet synchronization guards
    // For layout see Synaptics PS/2 TouchPad Interfacing Guide page 23
    constexpr u8 SYN_SYNC_MASK = 0xC8; // check bits 7, 6, 3
    constexpr u8 SYN_BYTE0_SYNC = 0x80; // buf[0]: bit7=1, bit6=0, bit3=0
    constexpr u8 SYN_BYTE3_SYNC = 0xC0; // buf[3]: bit7=1, bit6=1, bit3=0

    // Pressure (Z) thresholds
    constexpr u8 SYN_Z_FINGER_DOWN = 25; // Minimum Z to count as finger contact
    constexpr u8 SYN_Z_PALM_MIN = 180; // Z above this → probable palm

    enum class SYNAPTICS_GEOMETRY : u8 {
        UNKNOWN = 0,
        RECTANGLE = 1,
        ROUND = 2,
        ROUNDED_RECTANGLE = 3,
        RACE_TRACK = 4
    };

    struct SYNAPTICS_IDENTIFY_FIELDS {
        u8 minor_version;
        u8 magic; // Expected: 0x47

        u8 major_version : 4;
        u8 model_code : 4;
    } __attribute__((packed));

    union SYNAPTICS_IDENTIFY_RESPONSE {
        u8 raw[3];
        SYNAPTICS_IDENTIFY_FIELDS fields;
    } __attribute__((packed));


    // ============================================================================
    // CAPABILITIES
    // ============================================================================

    struct SYNAPTICS_CAPABILITIES_BYTE0_NEW {
        u8 reserved0 : 2;
        u8 has_middle_button : 1;
        u8 reserved1 : 1;
        u8 num_ext_queries : 3;
        u8 has_extended_caps : 1;
    } __attribute__((packed));

    struct SYNAPTICS_CAPABILITIES_BYTE2 {
        u8 has_palm_detect : 1;
        u8 has_multi_finger : 1;
        u8 has_ballistics : 1;
        u8 has_four_buttons : 1;
        u8 has_sleep_mode : 1;
        u8 has_multi_finger_report : 1;
        u8 has_low_power_mode : 1;
        u8 has_pass_through : 1;
    } __attribute__((packed));

    struct SYNAPTICS_CAPABILITIES_NEW {
        SYNAPTICS_CAPABILITIES_BYTE0_NEW byte0;
        u8 model_sub_number;
        SYNAPTICS_CAPABILITIES_BYTE2 byte2;
    } __attribute__((packed));

    struct SYNAPTICS_CAPABILITIES_OLD {
        u8 reserved0 : 1;
        u8 num_ext_queries : 3;
        u8 has_extended_caps : 1;
        u8 has_middle_button : 1;
        u8 reserved1 : 2;

        u8 magic; // Expected: 0x47

        SYNAPTICS_CAPABILITIES_BYTE2 byte2;
    } __attribute__((packed));

    union SYNAPTICS_CAPABILITIES_RESPONSE {
        u8 raw[3];

        SYNAPTICS_CAPABILITIES_NEW modern;
        SYNAPTICS_CAPABILITIES_OLD legacy;
    } __attribute__((packed));


    // ============================================================================
    // MODEL ID
    // ============================================================================

    struct SYNAPTICS_MODEL_ID_FIELDS {
        // Byte 0
        u8 sensor_type : 6;
        u8 supports_portrait : 1;
        u8 supports_rot180 : 1;

        // Byte 1
        u8 reserved0 : 1;
        u8 hardware_revision : 7;

        // Byte 2
        SYNAPTICS_GEOMETRY geometry : 4;
        u8 reserved1 : 1;
        u8 supports_simple_cmd : 1;
        u8 reserved2 : 1;
        u8 supports_new_abs : 1;
    } __attribute__((packed));

    union SYNAPTICS_MODEL_ID_RESPONSE {
        u8 raw[3];
        SYNAPTICS_MODEL_ID_FIELDS fields;
    } __attribute__((packed));


    // ============================================================================
    // EXTENDED MODEL
    // ============================================================================

    struct SYNAPTICS_EXT_MODEL_BYTE0 {
        u8 has_vertical_scroll : 1;
        u8 has_horizontal_scroll : 1;
        u8 supports_ext_w_mode : 1;
        u8 has_vertical_wheel : 1;
        u8 has_glass_pass : 1;
        u8 has_peak_detect : 1;
        u8 has_light_control : 1;
        u8 reserved : 1;
    } __attribute__((packed));

    struct SYNAPTICS_EXT_MODEL_BYTE1 {
        u8 reserved : 2;
        u8 ext_sensor_type : 2;
        u8 num_ext_buttons : 4;
    } __attribute__((packed));

    struct SYNAPTICS_EXT_MODEL_FIELDS {
        SYNAPTICS_EXT_MODEL_BYTE0 byte0;
        SYNAPTICS_EXT_MODEL_BYTE1 byte1;
        u8 product_id;
    } __attribute__((packed));

    union SYNAPTICS_EXT_MODEL_RESPONSE {
        u8 raw[3];
        SYNAPTICS_EXT_MODEL_FIELDS fields;
    } __attribute__((packed));

    // ============================================================================
    // CONTINUED CAPS
    // ============================================================================

    struct SYNAPTICS_CONTINUED_CAPS_BYTE0 {
        u8 tb_adj_thresh : 1; // Bit 0
        u8 reports_max : 1; // Bit 1
        u8 clear_pad : 1; // Bit 2
        u8 advanced_gestures : 1; // Bit 3
        u8 clk_pad_bit0 : 1; // Bit 4
        u8 multi_finger_mode : 2; // Bits 6:5
        u8 covered_pad_gest : 1; // Bit 7
    } __attribute__((packed));

    struct SYNAPTICS_CONTINUED_CAPS_BYTE1 {
        u8 clk_pad_bit1 : 1; // Bit 0
        u8 deluxe_leds : 1; // Bit 1
        u8 no_abs_pos_filt : 1; // Bit 2
        u8 reports_v : 1; // Bit 3
        u8 uniform_clk_pad : 1; // Bit 4
        u8 reports_min : 1; // Bit 5
        u8 inter_touch : 1; // Bit 6
        u8 reserved : 1; // Bit 7
    } __attribute__((packed));

    union SYNAPTICS_CONTINUED_CAPS_RESPONSE {
        struct {
            SYNAPTICS_CONTINUED_CAPS_BYTE0 byte0;
            SYNAPTICS_CONTINUED_CAPS_BYTE1 byte1;
            u8 inter_touch_i2c_addr; // Byte 2: InterTouch I²C address
        } __attribute__((packed));

        u8 raw[3];
    } __attribute__((packed));

    // ============================================================================
    // COORDINATE RANGE (Queries $0D / $0F)
    // ============================================================================

    struct SYNAPTICS_COORD_RESPONSE_FIELDS {
    } __attribute__((packed));

    union SYNAPTICS_COORD_RESPONSE {
        struct {
            u8 x_hi; // X coordinate bits [12:5]

            u8 y_lo : 4; // Y[4:1]
            u8 x_lo : 4; // X[4:1]

            u8 y_hi; // Y coordinate bits [12:5]
        }__attribute__((packed));;

        u8 raw[3];

        [[nodiscard]] constexpr u16 x() const {
            return (static_cast<u16>(x_hi) << 4) | static_cast<u16>(x_lo);
        }

        [[nodiscard]] constexpr u16 y() const {
            return (static_cast<u16>(y_hi) << 4) | static_cast<u16>(y_lo);
        }
    } __attribute__((packed));


    // ============================================================================
    // STANDARD MOTION PACKET
    // ============================================================================

    struct SYNAPTICS_PACKET_BYTE0 {
        u8 left_button : 1;
        u8 right_button : 1;
        u8 w_bit1 : 1;
        u8 sync0 : 1; // Always 0
        u8 w_bits3_2 : 2;
        u8 sync1 : 2; // Always 01
    } __attribute__((packed));

    struct SYNAPTICS_MOTION_BYTE1 {
        u8 x_pos_11_8 : 4;
        u8 y_pos_11_8 : 4;
    } __attribute__((packed));

    struct SYNAPTICS_MOTION_BYTE3 {
        u8 ext_left_up : 1;
        u8 ext_right_down : 1;
        u8 w_bit0 : 1;
        u8 sync0 : 1; // Always 0
        u8 x_pos_12 : 1;
        u8 y_pos_12 : 1;
        u8 sync1 : 2; // Always 11
    } __attribute__((packed));

    union SYNAPTICS_MOTION_PACKET {
        struct {
            SYNAPTICS_PACKET_BYTE0 byte0;
            SYNAPTICS_MOTION_BYTE1 byte1;

            u8 pressure_z;

            SYNAPTICS_MOTION_BYTE3 byte3;

            u8 x_pos_7_0;
            u8 y_pos_7_0;
        }__attribute__((packed));

        u8 raw[6];
    } __attribute__((packed));

    // ----------------------------------------------------------------------------
    // EW TYPE 0 - Scroll Packet
    // ----------------------------------------------------------------------------

    union SYNAPTICS_EW_SCROLL_PACKET {
        struct {
            SYNAPTICS_PACKET_BYTE0 byte0;

            i8 wheel1_delta;
            i8 wheel2_delta;

            u8 ext_left_button : 1;
            u8 ext_right_button : 1;
            u8 sync0 : 2;
            u8 wheel4_hi : 2;
            u8 sync1 : 2;

            i8 wheel3_delta;

            u8 wheel4_lo : 4;
            SYNAPTICS_EW_PACKET_TYPE type : 4; // 0
        }__attribute__((packed));

        u8 raw[6];
    } __attribute__((packed));


    // ----------------------------------------------------------------------------
    // EW TYPE 1 - Secondary Finger Packet
    // ----------------------------------------------------------------------------

    union SYNAPTICS_EW_SECOND_FINGER_PACKET {
        struct {
            SYNAPTICS_PACKET_BYTE0 byte0;

            u8 x_pos_lo;
            u8 y_pos_lo;

            u8 ext_left_button : 1;
            u8 ext_right_button : 1;
            u8 sync0 : 2;
            u8 z_hi : 2;
            u8 sync1 : 2;

            u8 x_pos_hi : 4;
            u8 y_pos_hi : 4;

            u8 z_lo : 4;
            SYNAPTICS_EW_PACKET_TYPE type : 4; // 1
        }__attribute__((packed));

        u8 raw[6];
    } __attribute__((packed));


    // ----------------------------------------------------------------------------
    // EW TYPE 2 - Finger State Packet
    // ----------------------------------------------------------------------------

    union SYNAPTICS_EW_FINGER_STATE_PACKET {
        struct {
            ;
            SYNAPTICS_PACKET_BYTE0 byte0;

            u8 finger_count : 4;
            u8 reserved0 : 4;

            u8 primary_finger_index;

            u8 middle_left_button : 1;
            u8 down_right_button : 1;
            u8 sync0 : 2;
            u8 reserved1 : 2;
            u8 sync1 : 2;

            u8 secondary_finger_index;

            u8 reserved2 : 4;
            SYNAPTICS_EW_PACKET_TYPE type : 4; // 2
        }__attribute__((packed));

        u8 raw[6];
    } __attribute__((packed));


    // ----------------------------------------------------------------------------
    // Generic EW Packet View
    // ----------------------------------------------------------------------------

    union SYNAPTICS_EW_PACKET {
        SYNAPTICS_EW_SCROLL_PACKET scroll;
        SYNAPTICS_EW_SECOND_FINGER_PACKET second_finger;
        SYNAPTICS_EW_FINGER_STATE_PACKET finger_state;
        u8 raw[6];
    } __attribute__((packed));


    // ============================================================================
    // TOP-LEVEL MOTION PACKET
    // ============================================================================

    union SYNAPTICS_PACKET {
        u8 raw[6];

        SYNAPTICS_MOTION_PACKET motion;
        SYNAPTICS_EW_PACKET extended_w;
    } __attribute__((packed));

    static_assert(sizeof(SYNAPTICS_PACKET) == 6);


    struct SynapticsInfo {
        SYNAPTICS_IDENTIFY_RESPONSE identity;
        SYNAPTICS_CAPABILITIES_RESPONSE capabilities;
        SYNAPTICS_MODEL_ID_RESPONSE model_id;
        SYNAPTICS_EXT_MODEL_RESPONSE ext_model;
        u8 firmware_major;
        u8 firmware_minor;

        bool has_coord_bounds;
        u16 x_min, x_max;
        u16 y_min, y_max;
        u16 soft_button_split_x; // x_min + (x_max - x_min) / 2

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

    bool initialize_guest();

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
