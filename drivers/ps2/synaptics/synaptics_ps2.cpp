// synaptics_ps2.cpp
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

#include "synaptics_ps2.h"

#include <vespera/cpu/io.h>
#include <vespera/graphics/display_manager.h>
#include <vespera/input/mice_device.h>
#include <vespera/types.h>

#include "../i8042.h"
#include "vespera/log.h"

namespace ps2::synaptics {
    static bool g_active = false;

    static u8 g_buf[6]{};
    static u8 g_cycle = 0;

    static u32 g_cursor_x = 0;
    static u32 g_cursor_y = 0;
    static bool g_cursor_initialised = false;

    static u32 g_tp_prev_x = 0;
    static u32 g_tp_prev_y = 0;
    static bool g_tp_first_packet = true;

    namespace {
        // Query Synaptics register via sliced commands and read response
        bool synaptics_query(const u8 query, u8 out[3]) {
            constexpr u8 STATUS_REQUEST = 0xE9;
            if (!i8042::aux_sliced_cmd(query)) return false;
            if (!i8042::aux_send(STATUS_REQUEST)) return false;
            // Three response bytes flow directly out of the controller after E9;
            // they are not individually ACK'd – just read them from DATA_PORT.
            i8042::wait_read();
            out[0] = inb(i8042::DATA_PORT);
            i8042::wait_read();
            out[1] = inb(i8042::DATA_PORT);
            i8042::wait_read();
            out[2] = inb(i8042::DATA_PORT);
            return true;
        }

        u32 clamp_u32(const i32 val, const u32 max) {
            if (max == 0) return 0;
            if (val < 0) return 0;
            if (static_cast<u32>(val) >= max) return max - 1;
            return static_cast<u32>(val);
        }

        void ensure_cursor_init() {
            if (g_cursor_initialised) return;
            const u32 w = DisplayManager::primary().drv->screen_width_px();
            const u32 h = DisplayManager::primary().drv->screen_height_px();
            g_cursor_x = w / 2;
            g_cursor_y = h / 2;
            g_cursor_initialised = true;
        }

        bool synaptics_firmware_at_least(const int major, const int minor, int req_major, int req_minor) {
            return major > req_major || (major == req_major && minor >= req_minor);
        }
    } // anonymous namespace

    bool probe(SynapticsInfo* const out_info) {
        i8042::drain();

        if (!synaptics_query(SYN_QUE_IDENTIFY, out_info->identity.raw)) return false;

        if (out_info->identity.fields.magic != SYN_IDENTITY_MAGIC) return false;

        out_info->firmware_major = out_info->identity.fields.info_major;
        out_info->firmware_minor = out_info->identity.fields.info_minor;

        if (!synaptics_query(SYN_QUE_CAPABILITIES, out_info->capabilities.raw)) return false;

        if (synaptics_firmware_at_least(out_info->firmware_major, out_info->firmware_minor, 7, 5)) {
            out_info->has_passthrough = out_info->capabilities.c_new.byte3.cap_pass_through;
        }
        else {
            out_info->has_passthrough = out_info->capabilities.c_old.byte3.cap_pass_through;
        }

        if (!synaptics_query(SYN_QUE_MODEL_ID, out_info->model_id.raw)) return false;

        return true;
    }

    bool tunnel_cmd(const u8 cmd) {
        constexpr u8 DISABLE = 0xF5;
        constexpr u8 SET_SAMPLE_RATE = 0xF3;
        constexpr u8 TUNNEL_COMMIT = 0x28;
        constexpr u8 ENABLE = 0xF4;

        if (!i8042::aux_send(DISABLE)) return false;
        if (!i8042::aux_sliced_cmd(cmd)) return false;
        if (!i8042::aux_send(SET_SAMPLE_RATE)) return false;
        if (!i8042::aux_send(TUNNEL_COMMIT)) return false;

        i8042::drain();

        if (!i8042::aux_send(ENABLE)) return false;
        return true;
    }

    bool set_mode(const u8 mode_byte) {
        constexpr u8 DISABLE = 0xF5;
        constexpr u8 SET_SAMPLE_RATE = 0xF3;
        constexpr u8 SET_SAMPLE_RATE_COMMIT = 0x14;

        if (!i8042::aux_send(DISABLE)) return false;
        if (!i8042::aux_sliced_cmd(mode_byte)) return false;
        if (!i8042::aux_send(SET_SAMPLE_RATE)) return false;
        if (!i8042::aux_send(SET_SAMPLE_RATE_COMMIT)) return false;

        // Re-enable streaming (the pad may have paused reporting during the mode
        // change sequence).
        constexpr u8 ENABLE_REPORTING = 0xF4;
        if (!i8042::aux_send(ENABLE_REPORTING)) return false;

        g_active = true;
        g_cycle = 0;
        return true;
    }

    bool is_active() {
        return g_active;
    }

    namespace {
        // Detect forwarded TrackPoint packet signature
        bool is_pt_packet(const u8* buf) {
            return (buf[0] & SYN_PT_BYTE0_MASK) == SYN_PT_BYTE0_VAL
                && (buf[3] & SYN_PT_BYTE3_MASK) == SYN_PT_BYTE3_VAL;
        }

        // Process forwarded TrackPoint relative motion
        void dispatch_trackpoint(const u8* buf) {
            const u8 status = buf[1];
            const u8 raw_dx = buf[4];
            const u8 raw_dy = buf[5];

            // Discard if either overflow flag is set.
            if ((status & 0x40) || (status & 0x80)) return;

            const bool x_neg = (status & 0x10) != 0;
            const bool y_neg = (status & 0x20) != 0;

            // Sign-extend 9-bit values; Y is inverted for screen space.
            const i32 dx = x_neg ? (static_cast<i32>(raw_dx) - 256) : static_cast<i32>(raw_dx);
            const i32 dy = y_neg ? (256 - static_cast<i32>(raw_dy)) : -static_cast<i32>(raw_dy);

            ensure_cursor_init();

            const u32 screen_w = DisplayManager::primary().drv->screen_width_px();
            const u32 screen_h = DisplayManager::primary().drv->screen_height_px();

            g_cursor_x = clamp_u32(static_cast<i32>(g_cursor_x) + dx, screen_w);
            g_cursor_y = clamp_u32(static_cast<i32>(g_cursor_y) + dy, screen_h);

            kernel::input::MouseButtonMask buttons = 0;
            if (status & 0x01) buttons |= static_cast<u8>(kernel::input::MouseButton::LEFT);
            if (status & 0x02) buttons |= static_cast<u8>(kernel::input::MouseButton::RIGHT);
            if (status & 0x04) buttons |= static_cast<u8>(kernel::input::MouseButton::MIDDLE);

            const kernel::input::InputEvent ev{
                .device = kernel::input::InputDeviceType::MOUSE,
                .mouse = {
                    .x = g_cursor_x,
                    .y = g_cursor_y,
                    .delta_x = dx,
                    .delta_y = dy,
                    .wheel_delta = 0,
                    .buttons_pressed = buttons,
                }
            };

            kernel::input::MiceDevice::share_mouse_event(ev);
        }

        // Process absolute touchpad packet and convert to relative offsets
        void dispatch_touchpad_abs(const u8* buf) {
            // ── Coordinate extraction (Wmode=1, New-Absolute format) ──────────
            const u32 x = (static_cast<u32>(buf[3] & 0x10) << 8) // X[12]
                | (static_cast<u32>(buf[1] & 0x0F) << 8) // X[11:8]
                | static_cast<u32>(buf[4]); // X[7:0]

            const u32 y = (static_cast<u32>(buf[3] & 0x20) << 7) // Y[12]
                | (static_cast<u32>(buf[1] & 0xF0) << 4) // Y[11:8]
                | static_cast<u32>(buf[5]); // Y[7:0]

            const u8 pressure = buf[2]; // Z – NOT buf[5]!

            // Buttons are in buf[0] for regular absolute packets.
            const bool left = (buf[0] & 0x01) != 0;
            const bool right = (buf[0] & 0x02) != 0;

            // No finger on pad – reset relative tracking.
            if (pressure < 25) {
                g_tp_first_packet = true;
                return;
            }

            // Anchor the first contact point; do not emit a delta yet.
            if (g_tp_first_packet) {
                g_tp_prev_x = x;
                g_tp_prev_y = y;
                g_tp_first_packet = false;
                return;
            }

            // Touchpad coordinate space increases Y upward; invert for screen.
            const i32 dx = static_cast<i32>(x) - static_cast<i32>(g_tp_prev_x);
            const i32 dy = static_cast<i32>(g_tp_prev_y) - static_cast<i32>(y);

            g_tp_prev_x = x;
            g_tp_prev_y = y;

            // Scale down from the Synaptics absolute space (~0..6143 × 0..4767)
            // to a sensible cursor-pixel delta.  Adjust TOUCHPAD_DIVISOR to taste.
            constexpr i32 TOUCHPAD_DIVISOR = 8;
            const i32 cdx = dx / TOUCHPAD_DIVISOR;
            const i32 cdy = dy / TOUCHPAD_DIVISOR;

            if (cdx == 0 && cdy == 0 && !left && !right) return;

            ensure_cursor_init();

            const u32 screen_w = DisplayManager::primary().drv->screen_width_px();
            const u32 screen_h = DisplayManager::primary().drv->screen_height_px();

            g_cursor_x = clamp_u32(static_cast<i32>(g_cursor_x) + cdx, screen_w);
            g_cursor_y = clamp_u32(static_cast<i32>(g_cursor_y) + cdy, screen_h);

            kernel::input::MouseButtonMask buttons = 0;
            if (left) buttons |= static_cast<u8>(kernel::input::MouseButton::LEFT);
            if (right) buttons |= static_cast<u8>(kernel::input::MouseButton::RIGHT);

            const kernel::input::InputEvent ev{
                .device = kernel::input::InputDeviceType::MOUSE,
                .mouse = {
                    .x = g_cursor_x,
                    .y = g_cursor_y,
                    .delta_x = cdx,
                    .delta_y = cdy,
                    .wheel_delta = 0,
                    .buttons_pressed = buttons,
                }
            };
            kernel::input::MiceDevice::share_mouse_event(ev);
        }
    } // anonymous namespace

    void handle_byte(const u8 data) {
        if (!g_active) return;

        // Verify packet boundaries using "New Absolute" sync bits
        if (g_cycle == 0) {
            // Waiting for a valid first byte.
            if ((data & SYN_SYNC_MASK) != SYN_BYTE0_SYNC) return;
        }
        else if (g_cycle == 3) {
            // Byte 3 must have its own sync signature.
            if ((data & SYN_SYNC_MASK) != SYN_BYTE3_SYNC) {
                // Stream de-sync; reset and try to resync from this byte.
                g_cycle = 0;
                if ((data & SYN_SYNC_MASK) == SYN_BYTE0_SYNC) {
                    g_buf[g_cycle++] = data;
                }
                return;
            }
        }

        g_buf[g_cycle++] = data;

        if (g_cycle < 6) return;

        g_cycle = 0;

        if (is_pt_packet(g_buf)) {
            dispatch_trackpoint(g_buf);
        }
        else {
            dispatch_touchpad_abs(g_buf);
        }
    }
} // namespace ps2::synaptics
