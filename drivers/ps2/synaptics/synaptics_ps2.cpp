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
#include "ps2/ps2_defs.h"
#include "vespera/log.h"

namespace ps2::synaptics {
    static bool g_active = false;
    static bool g_has_ew_mode = false; // Cached from SynapticsInfo at set_mode time
    static bool g_has_multi = false;
    static bool g_has_palm = false;

    static u8 g_buf[6]{};
    static u8 g_cycle = 0;

    // Cursor position shared by touchpad and TrackPoint dispatchers
    static u32 g_cursor_x = 0;
    static u32 g_cursor_y = 0;
    static bool g_cursor_initialised = false;

    // Touchpad single-finger relative tracking state
    static u32 g_tp_prev_x = 0;
    static u32 g_tp_prev_y = 0;
    static bool g_tp_first_packet = true;

    namespace {
        bool synaptics_query(const u8 query, u8 out[3]) {
            if (!i8042::aux_sliced_cmd(query)) return false;
            if (!i8042::aux_send(CMD_STATUS_REQUEST)) return false;
            // Three response bytes flow out without individual ACKs
            i8042::wait_read();
            out[0] = inb(DATA_PORT);
            i8042::wait_read();
            out[1] = inb(DATA_PORT);
            i8042::wait_read();
            out[2] = inb(DATA_PORT);
            return true;
        }

        u32 clamp_u32(const i32 val, const u32 max) {
            if (max == 0 || val < 0) return 0;
            if (static_cast<u32>(val) >= max) return max - 1;
            return static_cast<u32>(val);
        }

        void ensure_cursor_init() {
            if (g_cursor_initialised) return;
            g_cursor_x = DisplayManager::primary().drv->screen_width_px() / 2;
            g_cursor_y = DisplayManager::primary().drv->screen_height_px() / 2;
            g_cursor_initialised = true;
        }

        bool firmware_at_least(const int major, const int minor,
                               const int req_major, const int req_minor) {
            return major > req_major || (major == req_major && minor >= req_minor);
        }

        // ── W-field extraction (spec §3.2.6, New-Absolute packet layout) ────
        //
        //   buf[0] bits [4:3] → W[2:1]
        //   buf[3] bit  [2]   → W[0]
        //
        u8 extract_w(const u8* buf) {
            return (((buf[0] >> 2) & 0x06) | ((buf[3] >> 2) & 0x01));
        }

        void emit_mouse_event(const i32 dx, const i32 dy,
                              const i8 wheel,
                              const kernel::input::MouseButtonMask buttons) {
            const kernel::input::InputEvent ev{
                .device = kernel::input::InputDeviceType::MOUSE,
                .mouse = {
                    .x = g_cursor_x,
                    .y = g_cursor_y,
                    .delta_x = dx,
                    .delta_y = dy,
                    .wheel_delta = wheel,
                    .buttons_pressed = buttons,
                }
            };
            kernel::input::MiceDevice::share_mouse_event(ev);
        }

        // ── Button mask from buf[0] (applies to all absolute packet types) ────
        kernel::input::MouseButtonMask buttons_from_byte0(const u8 b) {
            kernel::input::MouseButtonMask m = 0;
            if (b & 0x01) m |= static_cast<u8>(kernel::input::MouseButton::LEFT);
            if (b & 0x02) m |= static_cast<u8>(kernel::input::MouseButton::RIGHT);
            return m;
        }

        void dispatch_trackpoint(const u8* buf) {
            const u8 status = buf[1];
            const u8 raw_dx = buf[4];
            const u8 raw_dy = buf[5];

            // Discard if either overflow flag is set
            if ((status & 0x40) || (status & 0x80)) return;

            const bool x_neg = (status & 0x10) != 0;
            const bool y_neg = (status & 0x20) != 0;

            const i32 dx = x_neg ? (static_cast<i32>(raw_dx) - 256) : static_cast<i32>(raw_dx);
            const i32 dy = y_neg ? (256 - static_cast<i32>(raw_dy)) : -static_cast<i32>(raw_dy);

            ensure_cursor_init();
            const u32 sw = DisplayManager::primary().drv->screen_width_px();
            const u32 sh = DisplayManager::primary().drv->screen_height_px();
            g_cursor_x = clamp_u32(static_cast<i32>(g_cursor_x) + dx, sw);
            g_cursor_y = clamp_u32(static_cast<i32>(g_cursor_y) + dy, sh);

            kernel::input::MouseButtonMask buttons = 0;
            if (status & 0x01) buttons |= static_cast<u8>(kernel::input::MouseButton::LEFT);
            if (status & 0x02) buttons |= static_cast<u8>(kernel::input::MouseButton::RIGHT);
            if (status & 0x04) buttons |= static_cast<u8>(kernel::input::MouseButton::MIDDLE);

            emit_mouse_event(dx, dy, 0, buttons);
        }

        void dispatch_two_finger_scroll(const u8* buf) {
            // Re-use the same X/Y coordinate extraction as the single-finger path.
            const u32 x = (static_cast<u32>(buf[3] & 0x10) << 8)
                | (static_cast<u32>(buf[1] & 0x0F) << 8)
                | static_cast<u32>(buf[4]);

            const u32 y = (static_cast<u32>(buf[3] & 0x20) << 7)
                | (static_cast<u32>(buf[1] & 0xF0) << 4)
                | static_cast<u32>(buf[5]);

            const u8 pressure = buf[2];

            if (pressure < SYN_Z_FINGER_DOWN) {
                g_tp_first_packet = true;
                return;
            }

            if (g_tp_first_packet) {
                g_tp_prev_x = x;
                g_tp_prev_y = y;
                g_tp_first_packet = false;
                return;
            }

            // Touchpad Y increases upward; positive dy means fingers moved up →
            // scroll up (negative wheel delta in most conventions, but here we
            // follow "natural" scrolling: fingers up → content follows → wheel > 0).
            const i32 raw_dy = static_cast<i32>(y) - static_cast<i32>(g_tp_prev_y);

            g_tp_prev_x = x;
            g_tp_prev_y = y;

            // Scale: Synaptics Y range ~4767 units, we want ~1 scroll tick per
            // ~100 units of movement.
            constexpr i32 SCROLL_DIVISOR = 100;
            const i8 wheel = static_cast<i8>(raw_dy / SCROLL_DIVISOR);

            if (wheel == 0) return;

            ensure_cursor_init();
            const auto buttons = buttons_from_byte0(buf[0]);
            emit_mouse_event(0, 0, wheel, buttons);
        }

        void dispatch_ew_scroll(const u8* buf) {
            const i8 wheel = static_cast<i8>(buf[1]);
            if (wheel == 0) return;

            ensure_cursor_init();
            const auto buttons = buttons_from_byte0(buf[0]);
            emit_mouse_event(0, 0, wheel, buttons);
        }


        void dispatch_touchpad_abs(const u8* buf, const u8 w) {
            const u32 x = (static_cast<u32>(buf[3] & 0x10) << 8)
                | (static_cast<u32>(buf[1] & 0x0F) << 8)
                | static_cast<u32>(buf[4]);

            const u32 y = (static_cast<u32>(buf[3] & 0x20) << 7)
                | (static_cast<u32>(buf[1] & 0xF0) << 4)
                | static_cast<u32>(buf[5]);

            const u8 pressure = buf[2];

            // Lift detection
            if (pressure < SYN_Z_FINGER_DOWN) {
                g_tp_first_packet = true;
                return;
            }

            // Palm rejection: wide contact + high pressure → ignore.
            // Only meaningful when capPalmDetect is set (W carries width).
            if (g_has_palm && w >= SYN_W_PALM_THRESHOLD && pressure > SYN_Z_PALM_MIN)
                return;

            // Anchor first contact; do not emit a delta yet.
            if (g_tp_first_packet) {
                g_tp_prev_x = x;
                g_tp_prev_y = y;
                g_tp_first_packet = false;
                return;
            }

            // Touchpad Y increases upward; invert for screen space.
            const i32 dx = static_cast<i32>(x) - static_cast<i32>(g_tp_prev_x);
            const i32 dy = static_cast<i32>(g_tp_prev_y) - static_cast<i32>(y);

            g_tp_prev_x = x;
            g_tp_prev_y = y;

            // Scale from Synaptics absolute space (~6143 × ~4767) to pixel deltas.
            constexpr i32 TOUCHPAD_DIVISOR = 8;
            const i32 cdx = dx / TOUCHPAD_DIVISOR;
            const i32 cdy = dy / TOUCHPAD_DIVISOR;

            const auto buttons = buttons_from_byte0(buf[0]);

            if (cdx == 0 && cdy == 0 && !buttons) return;

            ensure_cursor_init();
            const u32 sw = DisplayManager::primary().drv->screen_width_px();
            const u32 sh = DisplayManager::primary().drv->screen_height_px();
            g_cursor_x = clamp_u32(static_cast<i32>(g_cursor_x) + cdx, sw);
            g_cursor_y = clamp_u32(static_cast<i32>(g_cursor_y) + cdy, sh);

            emit_mouse_event(cdx, cdy, 0, buttons);
        }

        void dispatch_packet(const u8* buf) {
            const u8 w = extract_w(buf);

            switch (w) {
            case SYN_W_PASSTHROUGH:
                // W=3 → TrackPoint encapsulation (spec §5.1)
                dispatch_trackpoint(buf);
                return;

            case SYN_W_EW_PACKET:
                // W=2 → Extended W encapsulation (spec §3.2.9), only if capEWmode
                if (g_has_ew_mode) {
                    const u8 ew_code = (buf[5] >> 4) & 0x0F;
                    if (ew_code == SYN_EW_CODE_SCROLL) {
                        dispatch_ew_scroll(buf);
                    }
                    // SYN_EW_CODE_SECOND_FINGER and SYN_EW_CODE_FINGER_STATE are
                    // not handled; we silently drop them.
                }
                return;

            case SYN_W_TWO_FINGERS: // fall-through
            case SYN_W_THREE_FINGERS:
                // W=0/1 → multi-finger; treat as two-finger scroll when capMultiFinger
                if (g_has_multi) {
                    dispatch_two_finger_scroll(buf);
                }
                else {
                    // Pad without capMultiFinger still sends W=0/1 but means nothing
                    // useful – fall back to single-finger cursor movement.
                    dispatch_touchpad_abs(buf, w);
                }
                return;

            default:
                // W=4..15 → single finger (width field when capPalmDetect)
                dispatch_touchpad_abs(buf, w);
                return;
            }
        }
    } // anonymous namespace

    bool probe(SynapticsInfo* const out_info) {
        i8042::drain();

        if (!synaptics_query(SYN_QUE_IDENTIFY, out_info->identity.raw)) return false;
        if (out_info->identity.fields.magic != SYN_IDENTITY_MAGIC) return false;

        out_info->firmware_major = out_info->identity.fields.info_major;
        out_info->firmware_minor = out_info->identity.fields.info_minor;

        if (!synaptics_query(SYN_QUE_CAPABILITIES, out_info->capabilities.raw)) return false;

        // Capability byte layout differs between firmware versions (spec §4.4)
        const bool new_caps = firmware_at_least(out_info->firmware_major,
                                                out_info->firmware_minor, 7, 5);
        const auto& b3 = new_caps
                             ? out_info->capabilities.c_new.byte3
                             : out_info->capabilities.c_old.byte3;

        out_info->has_passthrough = b3.cap_pass_through;
        out_info->has_multi_finger = b3.cap_multi_finger;
        out_info->has_palm_detect = b3.cap_palm_detect;

        if (!synaptics_query(SYN_QUE_MODEL_ID, out_info->model_id.raw)) return false;

        // Extended Model ID query is available when nExtendedQueries >= 1 (query 0x09)
        const u8 n_ext = new_caps
                             ? out_info->capabilities.c_new.byte1.n_extended_queries
                             : out_info->capabilities.c_old.n_extended_queries;

        out_info->has_ew_mode = false;
        if (n_ext >= 1) {
            if (synaptics_query(SYN_QUE_EXTENDED_MODEL, out_info->ext_model.raw)) {
                out_info->has_ew_mode = (out_info->ext_model.fields.byte0.ext_w_mode != 0);
            }
        }

        return true;
    }

    bool tunnel_cmd(const u8 cmd) {
        if (!i8042::aux_send(CMD_DISABLE)) return false;
        if (!i8042::aux_sliced_cmd(cmd)) return false;
        if (!i8042::aux_send(CMD_SET_SAMPLE_RATE)) return false;
        if (!i8042::aux_send(SYN_SAMPLE_RATE_TUNNEL_COMMIT)) return false;
        i8042::drain();
        if (!i8042::aux_send(CMD_ENABLE)) return false;
        return true;
    }

    bool set_mode(const u8 mode_byte) {
        if (!i8042::aux_send(CMD_DISABLE)) return false;
        if (!i8042::aux_sliced_cmd(mode_byte)) return false;
        if (!i8042::aux_send(CMD_SET_SAMPLE_RATE)) return false;
        if (!i8042::aux_send(SYN_SAMPLE_RATE_MODE_COMMIT)) return false;
        if (!i8042::aux_send(CMD_ENABLE)) return false;

        g_active = true;
        g_cycle = 0;
        return true;
    }

    bool is_active() {
        return g_active;
    }

    void set_caps(const SynapticsInfo& info) {
        g_has_ew_mode = info.has_ew_mode;
        g_has_multi = info.has_multi_finger;
        g_has_palm = info.has_palm_detect;
    }

    void handle_byte(const u8 data) {
        if (!g_active) return;

        // Packet boundary verification using New-Absolute sync bits
        if (g_cycle == 0) {
            if ((data & SYN_SYNC_MASK) != SYN_BYTE0_SYNC) return;
        }
        else if (g_cycle == 3) {
            if ((data & SYN_SYNC_MASK) != SYN_BYTE3_SYNC) {
                // De-sync: reset and try to recover from this byte
                g_cycle = 0;
                if ((data & SYN_SYNC_MASK) == SYN_BYTE0_SYNC)
                    g_buf[g_cycle++] = data;
                return;
            }
        }

        g_buf[g_cycle++] = data;
        if (g_cycle < 6) return;

        g_cycle = 0;
        dispatch_packet(g_buf);
    }
} // namespace ps2::synaptics
