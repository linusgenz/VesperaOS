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

namespace ps2::synaptics {
    struct SynapticsState {
        bool driver_initialised = false;
        bool active = false;

        bool has_ew_mode = false;
        bool has_multi = false;
        bool has_palm = false;
        bool has_passthrough = false;

        u16 x_min = 1472, x_max = 5472;
        u16 y_min = 1408, y_max = 4448;
        u16 soft_split_x = 0;

        u32 cursor_x = 0;
        u32 cursor_y = 0;

        u32 tp_prev_x = 0;
        u32 tp_prev_y = 0;
        bool tp_first_packet = true;

        u32 scroll_prev_x = 0;
        u32 scroll_prev_y = 0;
        bool scroll_first_packet = true;

        bool soft_button_down = false;

        u32 screen_width = 0;
        u32 screen_height = 0;

        SYNAPTICS_PACKET pkt{};
        u8 cycle = 0;
    };

    static SynapticsState g;

    namespace {
        // ── read_encapsulation_packet ─────────────────────────────────────────
        // Reads a single 6-byte W=3 encapsulation packet from the AUX port
        // (spec §5.1.5, Figure 5-3) and extracts the four guest payload bytes.
        //
        // Encapsulation layout:
        //   enc[0]  host status  (bits: 1,0,0,0,0,1,R,L)
        //   enc[1]  Guest packet byte 1   ← primary response byte (usually $FA)
        //   enc[2]  Guest packet byte 4   (= $00 for 3-byte guests)
        //   enc[3]  host status  (bits: 1,1,rsvd,0,1,rsvd,R,L)
        //   enc[4]  Guest packet byte 2
        //   enc[5]  Guest packet byte 3
        //
        // Returns false if any byte times out.
        // ─────────────────────────────────────────────────────────────────────
        bool read_encapsulation_packet(u8 out_guest[4]) {
            u8 enc[6];
            for (auto& b : enc) {
                if (!i8042::wait_read()) return false;
                b = inb(DATA_PORT);
            }
            // Reconstruct the original guest packet bytes in canonical order
            out_guest[0] = enc[1]; // byte 1 of guest packet
            out_guest[1] = enc[4]; // byte 2
            out_guest[2] = enc[5]; // byte 3
            out_guest[3] = enc[2]; // byte 4 ($00 for 3-byte guests)
            return true;
        }

        bool synaptics_query(const u8 query, u8 out[3]) {
            if (!i8042::aux_sliced_cmd(query)) return false;
            if (!i8042::aux_send(CMD_STATUS_REQUEST)) return false;
            // Three response bytes flow out without individual ACKs
            if (!i8042::wait_read()) return false;
            out[0] = inb(DATA_PORT);
            if (!i8042::wait_read()) return false;
            out[1] = inb(DATA_PORT);
            if (!i8042::wait_read()) return false;
            out[2] = inb(DATA_PORT);
            return true;
        }

        u32 clamp_u32(const i32 val, const u32 max) {
            if (max == 0 || val < 0) return 0;
            if (static_cast<u32>(val) >= max) return max - 1;
            return static_cast<u32>(val);
        }

        void display_changed(const DisplayInfo& info, void*) {
            g.screen_width = info.width;
            g.screen_height = info.height;
        }

        bool firmware_at_least(
            const int major, const int minor,
            const int req_major, const int req_minor
        ) {
            return major > req_major || (major == req_major && minor >= req_minor);
        }

        u8 extract_w(const SYNAPTICS_PACKET* pkt) {
            return (pkt->motion.byte0.w_bits3_2 << 2) |
                (pkt->motion.byte0.w_bit1 << 1) |
                pkt->motion.byte3.w_bit0;
        }

        void emit_mouse_event(
            const i32 dx, const i32 dy, const i8 wheel, const kernel::input::MouseButtonMask buttons
        ) {
            const kernel::input::InputEvent ev{
                .device = kernel::input::InputDeviceType::MOUSE,
                .mouse = {
                    .x = g.cursor_x,
                    .y = g.cursor_y,
                    .delta_x = dx,
                    .delta_y = dy,
                    .wheel_delta = wheel,
                    .buttons_pressed = buttons,
                }
            };
            kernel::input::MiceDevice::share_mouse_event(ev);
        }

        // ── Button mask from buf[0] (applies to all absolute packet types) ────
        kernel::input::MouseButtonMask buttons_from_byte0(const SYNAPTICS_PACKET_BYTE0 b) {
            kernel::input::MouseButtonMask m = 0;
            if (b.left_button) m |= static_cast<u8>(kernel::input::MouseButton::LEFT);
            if (b.right_button) m |= static_cast<u8>(kernel::input::MouseButton::RIGHT);
            return m;
        }

        kernel::input::MouseButtonMask buttons_with_soft_click(
            const SYNAPTICS_PACKET_BYTE0 byte0, const SYNAPTICS_MOTION_BYTE3 byte3,
            const u32 x
        ) {
            kernel::input::MouseButtonMask buttons = buttons_from_byte0(byte0);
            if (byte3.ext_left_up) {
                g.soft_button_down = true;

                if (x < g.soft_split_x)
                    buttons |= static_cast<u8>(kernel::input::MouseButton::LEFT);
                else
                    buttons |= static_cast<u8>(kernel::input::MouseButton::RIGHT);
            } else if (g.soft_button_down) {
                g.soft_button_down = false;
                emit_mouse_event(0, 0, 0, 0);
            }

            return buttons;
        }

        void dispatch_trackpoint(const SYNAPTICS_RELATIVE_PACKET& pkt) {
            const SYNAPTICS_RELATIVE_BYTE0 status = pkt.byte0;
            const u8 raw_dx = pkt.x_delta;
            const u8 raw_dy = pkt.y_delta;

            // Discard if either overflow flag is set
            if (status.x_overflow || status.y_overflow)
                return;

            const bool x_neg = status.x_sign;
            const bool y_neg = status.y_sign;

            const i32 dx = x_neg ? (static_cast<i32>(raw_dx) - 256) : static_cast<i32>(raw_dx);
            const i32 dy = y_neg ? (256 - static_cast<i32>(raw_dy)) : -static_cast<i32>(raw_dy);

            g.cursor_x = clamp_u32(static_cast<i32>(g.cursor_x) + dx, g.screen_width);
            g.cursor_y = clamp_u32(static_cast<i32>(g.cursor_y) + dy, g.screen_height);

            kernel::input::MouseButtonMask buttons = 0;
            if (status.left_button) buttons |= static_cast<u8>(kernel::input::MouseButton::LEFT);
            if (status.right_button) buttons |= static_cast<u8>(kernel::input::MouseButton::RIGHT);
            if (status.middle_button) buttons |= static_cast<u8>(kernel::input::MouseButton::MIDDLE);

            emit_mouse_event(dx, dy, 0, buttons);
        }

        void dispatch_two_finger_scroll(const SYNAPTICS_MOTION_PACKET* pkt) {
            // Re-use the same X/Y coordinate extraction as the single-finger path.
            const u32 x = (static_cast<u32>(pkt->byte3.x_pos_12) << 12)
                | (static_cast<u32>(pkt->byte1.x_pos_11_8) << 8)
                | static_cast<u32>(pkt->x_pos_7_0);

            const u32 y = (static_cast<u32>(pkt->byte3.y_pos_12) << 12)
                | (static_cast<u32>(pkt->byte1.y_pos_11_8) << 8)
                | static_cast<u32>(pkt->y_pos_7_0);

            const u8 pressure = pkt->pressure_z;

            if (pressure < SYN_Z_FINGER_DOWN) {
                g.scroll_first_packet = true;
                return;
            }

            if (g.scroll_first_packet) {
                g.scroll_prev_x = x;
                g.scroll_prev_y = y;
                g.scroll_first_packet = false;
                return;
            }

            // Touchpad Y increases upward; positive dy means fingers moved up →
            // scroll up (negative wheel delta in most conventions, but here we
            // follow "natural" scrolling: fingers up → content follows → wheel > 0).
            const i32 raw_dy = static_cast<i32>(y) - static_cast<i32>(g.scroll_prev_y);

            g.scroll_prev_x = x; // TODO implement horizontal scroll later
            g.scroll_prev_y = y;

            // Scale: Synaptics Y range ~4767 units, we want ~1 scroll tick per
            // ~100 units of movement.
            constexpr i32 SCROLL_DIVISOR = 100;
            const i8 wheel = static_cast<i8>(raw_dy / SCROLL_DIVISOR);

            if (wheel == 0) return;

            const auto buttons = buttons_with_soft_click(pkt->byte0, pkt->byte3, x);
            emit_mouse_event(0, 0, wheel, buttons);
        }

        void dispatch_ew_scroll(const SYNAPTICS_EW_SCROLL_PACKET& pkt) {
            const i8 wheel = pkt.wheel1_delta;
            if (wheel == 0) return;

            const auto buttons = buttons_from_byte0(pkt.byte0);
            emit_mouse_event(0, 0, wheel, buttons);
        }

        void dispatch_touchpad_abs(const SYNAPTICS_MOTION_PACKET* pkt, const u8 w) {
            const u32 x = (static_cast<u32>(pkt->byte3.x_pos_12) << 12)
                | (static_cast<u32>(pkt->byte1.x_pos_11_8) << 8)
                | static_cast<u32>(pkt->x_pos_7_0);

            const u32 y = (static_cast<u32>(pkt->byte3.y_pos_12) << 12)
                | (static_cast<u32>(pkt->byte1.y_pos_11_8) << 8)
                | static_cast<u32>(pkt->y_pos_7_0);

            const u8 pressure = pkt->pressure_z;

            // Lift detection
            if (pressure < SYN_Z_FINGER_DOWN) {
                if (g.soft_button_down) {
                    g.soft_button_down = false;
                    emit_mouse_event(0, 0, 0, 0);
                }
                g.tp_first_packet = true;
                g.scroll_first_packet = true;
                return;
            }

            // Palm rejection: wide contact + high pressure → ignore.
            // Only meaningful when capPalmDetect is set (W carries width).
            if (g.has_palm && w >= SYN_W_PALM_THRESHOLD && pressure > SYN_Z_PALM_MIN) return;

            // Anchor first contact; do not emit a delta yet.
            if (g.tp_first_packet) {
                g.tp_prev_x = x;
                g.tp_prev_y = y;
                g.tp_first_packet = false;
                return;
            }

            // Touchpad Y increases upward; invert for screen space.
            const i32 dx = static_cast<i32>(x) - static_cast<i32>(g.tp_prev_x);
            const i32 dy = static_cast<i32>(g.tp_prev_y) - static_cast<i32>(y);

            g.tp_prev_x = x;
            g.tp_prev_y = y;

            // Scale from Synaptics absolute space (~6143 × ~4767) to pixel deltas.
            constexpr i32 TOUCHPAD_DIVISOR = 8;
            const i32 cdx = dx / TOUCHPAD_DIVISOR;
            const i32 cdy = dy / TOUCHPAD_DIVISOR;

            const auto buttons = buttons_with_soft_click(pkt->byte0, pkt->byte3, x);

            if (cdx == 0 && cdy == 0 && !buttons) return;

            g.cursor_x = clamp_u32(static_cast<i32>(g.cursor_x) + cdx, g.screen_width);
            g.cursor_y = clamp_u32(static_cast<i32>(g.cursor_y) + cdy, g.screen_height);

            emit_mouse_event(cdx, cdy, 0, buttons);
        }

        void dispatch_packet(const SYNAPTICS_PACKET* pkt) {
            switch (const u8 w = extract_w(pkt)) {
            case SYN_W_PASSTHROUGH: {
                if (!g.has_passthrough) return;

                dispatch_trackpoint(pkt->passthrough.passthrough_to_relative());
                return;
            }
            case SYN_W_EW_PACKET:
                // W=2 → Extended W encapsulation (spec §3.2.9), only if capEWmode
                if (g.has_ew_mode) {
                    if (pkt->extended_w.scroll.type == SYNAPTICS_EW_PACKET_SCROLL) {
                        dispatch_ew_scroll(pkt->extended_w.scroll);
                    }
                    // SYN_EW_CODE_SECOND_FINGER and SYN_EW_CODE_FINGER_STATE are
                    // not handled; we silently drop them.
                }
                return;

            case SYN_W_TWO_FINGERS:
            case SYN_W_THREE_FINGERS:
                // W=0/1 → multi-finger; treat as two-finger scroll when capMultiFinger
                if (g.has_multi) {
                    dispatch_two_finger_scroll(&pkt->motion);
                } else {
                    // Pad without capMultiFinger still sends W=0/1 but means nothing
                    // useful – fall back to single-finger cursor movement.
                    dispatch_touchpad_abs(&pkt->motion, w);
                }
                return;

            default:
                dispatch_touchpad_abs(&pkt->motion, w);
            }
        }

        i32 tunnel_cmd_ex(const u8 cmd) {
            if (!g.driver_initialised) return -1;

            if (!i8042::aux_send(CMD_DISABLE)) return -1;
            if (!i8042::aux_sliced_cmd(cmd)) {
                i8042::aux_send(CMD_ENABLE);
                return -1;
            }
            if (!i8042::aux_send(CMD_SET_SAMPLE_RATE)) {
                i8042::aux_send(CMD_ENABLE);
                return -1;
            }
            if (!i8042::aux_send(SYN_SAMPLE_RATE_TUNNEL_COMMIT)) {
                i8042::aux_send(CMD_ENABLE);
                return -1;
            }

            u8 guest_payload[4]{};
            if (!read_encapsulation_packet(guest_payload)) {
                i8042::aux_send(CMD_ENABLE);
                return -1;
            }

            if (!i8042::aux_send(CMD_ENABLE)) return -1;
            return guest_payload[0];
        }
    } // anonymous namespace

    void init() {
        DisplayManager::register_backend_hook(display_changed, nullptr, HookFlags::FIRE_IMMEDIATELY);

        g.cursor_x = g.screen_width / 2;
        g.cursor_y = g.screen_height / 2;
        g.driver_initialised = true;
    }

    bool probe(SynapticsInfo* const out_info) {
        if (!g.driver_initialised) return false;
        i8042::drain();

        if (!synaptics_query(SYN_QUE_IDENTIFY, out_info->identity.raw)) return false;
        if (out_info->identity.fields.magic != SYN_IDENTITY_MAGIC) return false;

        out_info->firmware_major = out_info->identity.fields.major_version;
        out_info->firmware_minor = out_info->identity.fields.minor_version;

        if (!synaptics_query(SYN_QUE_CAPABILITIES, out_info->capabilities.raw)) return false;

        // Capability byte layout differs between firmware versions (spec §4.4)
        const bool new_caps = firmware_at_least(out_info->firmware_major,
                                                out_info->firmware_minor, 7, 5);

        const auto& b3 = new_caps
                             ? out_info->capabilities.modern.byte2
                             : out_info->capabilities.legacy.byte2;

        out_info->has_passthrough = b3.has_pass_through;
        out_info->has_multi_finger = b3.has_multi_finger;
        out_info->has_palm_detect = b3.has_palm_detect;

        if (!synaptics_query(SYN_QUE_MODEL_ID, out_info->model_id.raw)) return false;

        // Extended Model ID query is available when nExtendedQueries >= 1 (query 0x09)
        const u8 n_ext = new_caps
                             ? out_info->capabilities.modern.byte0.num_ext_queries
                             : out_info->capabilities.legacy.num_ext_queries;

        out_info->has_ew_mode = false;
        if (n_ext >= 1) {
            if (synaptics_query(SYN_QUE_EXTENDED_MODEL, out_info->ext_model.raw)) {
                out_info->has_ew_mode = (out_info->ext_model.fields.byte0.supports_ext_w_mode != 0);
            }
        }

        bool reports_max = false;
        bool reports_min = false;
        if (n_ext >= 4) {
            SYNAPTICS_CONTINUED_CAPS_RESPONSE cont_caps{};
            if (synaptics_query(SYN_QUE_CONTINUED_CAPS, cont_caps.raw)) {
                reports_max = cont_caps.byte0.reports_max != 0;
                reports_min = cont_caps.byte1.reports_min != 0;
            }
        }

        out_info->has_coord_bounds = false;
        out_info->x_min = 0;
        out_info->x_max = 6143;
        out_info->y_min = 0;
        out_info->y_max = 4767;

        if (reports_max) {
            SYNAPTICS_COORD_RESPONSE resp{};
            if (synaptics_query(SYN_QUE_MAX_COORDS, resp.raw)) {
                out_info->x_max = resp.x();
                out_info->y_max = resp.y();
                out_info->has_coord_bounds = true;
            }
        }
        if (reports_min) {
            SYNAPTICS_COORD_RESPONSE resp{};
            if (synaptics_query(SYN_QUE_MIN_COORDS, resp.raw)) {
                out_info->x_min = resp.x();
                out_info->y_min = resp.y();
                out_info->has_coord_bounds = true;
            }
        }

        out_info->soft_button_split_x = out_info->x_min + (out_info->x_max - out_info->x_min) / 2;

        return true;
    }

    bool initialize_guest() {
        i32 device_id = tunnel_cmd_ex(CMD_GET_DEVICE_ID);
        if (device_id < 0) {
            return false;
        }

        // 2. Enable the guest device (TrackPoint) via the $F4 tunnel (CMD_ENABLE)
        if (tunnel_cmd_ex(CMD_ENABLE) < 0) return false;

        g.has_passthrough = true;
        return true;
    }

    bool set_mode(const u8 mode_byte) {
        if (!g.driver_initialised) return false;

        if (!i8042::aux_send(CMD_DISABLE))
            return false;

        bool ok = false;

        if (!i8042::aux_sliced_cmd(mode_byte)) goto cleanup;
        if (!i8042::aux_send(CMD_SET_SAMPLE_RATE)) goto cleanup;
        if (!i8042::aux_send(SYN_SAMPLE_RATE_MODE_COMMIT)) goto cleanup;

        ok = true;

    cleanup:
        i8042::aux_send(CMD_ENABLE);

        if (ok) {
            g.active = true;
            g.cycle = 0;

            g.tp_first_packet = true;
            g.scroll_first_packet = true;
        }

        return ok;
    }

    bool is_active() {
        return g.active;
    }

    void set_caps(const SynapticsInfo& info) {
        g.has_ew_mode = info.has_ew_mode;
        g.has_multi = info.has_multi_finger;
        g.has_palm = info.has_palm_detect;

        g.x_min = info.x_min;
        g.x_max = info.x_max;
        g.y_min = info.y_min;
        g.y_max = info.y_max;

        g.soft_split_x = info.soft_button_split_x;
    }

    void handle_byte(const u8 data) {
        if (!g.driver_initialised) return;
        if (!g.active) return;

        // Packet boundary verification using New-Absolute sync bits
        if (g.cycle == 0) {
            if ((data & SYN_SYNC_MASK) != SYN_BYTE0_SYNC) return;
        } else if (g.cycle == 3) {
            if ((data & SYN_SYNC_MASK) != SYN_BYTE3_SYNC) {
                // De-sync: reset and try to recover from this byte
                g.cycle = 0;

                if ((data & SYN_SYNC_MASK) == SYN_BYTE0_SYNC) {
                    g.pkt.raw[g.cycle++] = data;
                }
                return;
            }
        }

        g.pkt.raw[g.cycle++] = data;
        if (g.cycle < 6) return;

        g.cycle = 0;

        dispatch_packet(&g.pkt);
    }
} // namespace ps2::synaptics
