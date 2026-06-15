// ps2_mouse.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 30.07.25.
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

#include <drivers/ps2/mouse.h>
#include <vespera/cpu/io.h>
#include <vespera/graphics/display_manager.h>
#include <vespera/input/mice_device.h>
#include "../synaptics/synaptics_ps2.h"
#include "ps2/i8042.h"
#include "vespera/log.h"

namespace {
    constexpr u8 PS2_LEFT_BUTTON = 0b00000001;
    constexpr u8 PS2_RIGHT_BUTTON = 0b00000010;
    constexpr u8 PS2_MIDDLE_BUTTON = 0b00000100;
    constexpr u8 PS2_ALWAYS_ONE = 0b00001000;
    constexpr u8 PS2_X_SIGN = 0b00010000;
    constexpr u8 PS2_Y_SIGN = 0b00100000;
    constexpr u8 PS2_X_OVERFLOW = 0b01000000;
    constexpr u8 PS2_Y_OVERFLOW = 0b10000000;

    constexpr u16 PS2_DATA_PORT = 0x60;
    constexpr u16 PS2_CMD_PORT = 0x64;

    // Mouse device ID returned after successful IntelliMouse activation
    constexpr u8 PS2_MOUSE_ID_STANDARD = 0x00;
    constexpr u8 PS2_MOUSE_ID_INTELLIMOUSE = 0x03;

    constexpr u8 PS2_CMD_ENABLE_AUX_PORT = 0xA8;
    constexpr u8 PS2_CMD_READ_CONFIG_BYTE = 0x20;
    constexpr u8 PS2_CMD_WRITE_CONFIG_BYTE = 0x60;

    constexpr u8 PS2_MOUSE_SET_DEFAULTS = 0xF6;
    constexpr u8 PS2_MOUSE_ENABLE_REPORTING = 0xF4;
    constexpr u8 PS2_MOUSE_GET_DEVICE_ID = 0xF2;
    constexpr u8 PS2_MOUSE_SET_SAMPLE_RATE = 0xF3;

    constexpr u8 PS2_SAMPLE_RATE_200 = 200;
    constexpr u8 PS2_SAMPLE_RATE_100 = 100;
    constexpr u8 PS2_SAMPLE_RATE_80 = 80;


    // Query the mouse device ID.  Returns the ID byte, or 0xFF on timeout.
    u8 query_device_id() {
        ps2::i8042::aux_send(PS2_MOUSE_GET_DEVICE_ID);
        ps2::i8042::wait_read();
        return inb(PS2_DATA_PORT);
    }
} // namespace

namespace ps2::mouse {
    point_t Ps2Mouse::position_{0, 0};
    point_t Ps2Mouse::position_old_{0, 0};
    u8 Ps2Mouse::packet_[4]{};
    u8 Ps2Mouse::cycle_{0};
    u8 Ps2Mouse::packet_size_{3};
    bool Ps2Mouse::packet_ready_{false};
    bool Ps2Mouse::first_byte_skipped_{false};

    point_t Ps2Mouse::get_position() {
        return position_;
    }

    void Ps2Mouse::init() {
        outb(PS2_CMD_PORT, PS2_CMD_ENABLE_AUX_PORT);

        i8042::wait_write();
        outb(PS2_CMD_PORT, PS2_CMD_READ_CONFIG_BYTE);
        i8042::wait_read();

        u8 status = inb(PS2_DATA_PORT);
        status |= 0b10;

        i8042::wait_write();
        outb(PS2_CMD_PORT, PS2_CMD_WRITE_CONFIG_BYTE);

        i8042::wait_write();
        outb(PS2_DATA_PORT, status);

        i8042::aux_send(PS2_MOUSE_SET_SAMPLE_RATE);
        i8042::aux_send(PS2_SAMPLE_RATE_200);

        i8042::aux_send(PS2_MOUSE_SET_SAMPLE_RATE);
        i8042::aux_send(PS2_SAMPLE_RATE_100);

        i8042::aux_send(PS2_MOUSE_SET_SAMPLE_RATE);
        i8042::aux_send(PS2_SAMPLE_RATE_80);

        i8042::aux_send(PS2_MOUSE_SET_DEFAULTS);
        i8042::aux_send(PS2_MOUSE_ENABLE_REPORTING);

        const u8 device_id = query_device_id();

        if (device_id == PS2_MOUSE_ID_INTELLIMOUSE) {
            packet_size_ = 4;
        }
        else {
            packet_size_ = 3;
        }

        i8042::aux_send(PS2_MOUSE_SET_SAMPLE_RATE);
        i8042::aux_send(PS2_SAMPLE_RATE_100);

        i8042::drain();

        synaptics::SynapticsInfo syn_info{};
        if (synaptics::probe(&syn_info)) {
            // Synaptics pad found.  Switch to absolute + high-rate mode.
            // Once set_mode() returns, every IRQ12 byte goes to
            // synaptics::handle_byte() and Ps2Mouse::handle_byte() is bypassed.
            if (synaptics::set_mode(synaptics::SYN_MODE_DEFAULT)) {
                Log::ok("ps2: Synaptics touchpad initialized (firmware v%u.%u, absolute mode enabled)",
                        syn_info.firmware_major, syn_info.firmware_minor);

                if (syn_info.has_passthrough) {
                    Log::info("ps2: Enabling TrackPoint via tunnel...");
                    if (synaptics::tunnel_cmd(0xF4)) {
                        Log::ok("ps2: TrackPoint enabled");
                    } else {
                        Log::error("ps2: Failed to enable TrackPoint");
                    }
                }
            }
        }
    }

    void Ps2Mouse::handle_byte(const u8 data) {
        if (synaptics::is_active()) {
            synaptics::handle_byte(data);
            return;
        }


        if (!first_byte_skipped_) {
            first_byte_skipped_ = true;
            return;
        }

        switch (cycle_) {
        case 0:
            // Bit 3 of the status byte is always set in a well-formed packet.
            // Ignore any byte that does not have it set so we stay aligned.
            if ((data & PS2_ALWAYS_ONE) == 0) return;
            packet_[0] = data;
            cycle_ = 1;
            break;
        case 1:
            packet_[1] = data;
            cycle_ = 2;
            break;
        case 2:
            packet_[2] = data;
            if (packet_size_ == 3) {
                packet_ready_ = true;
                cycle_ = 0;
            }
            else {
                cycle_ = 3;
            }
            break;
        case 3:
            packet_[3] = data;
            packet_ready_ = true;
            cycle_ = 0;
            break;
        default:
            cycle_ = 0;
            break;
        }

        if (packet_ready_) {
            process_packet();
            packet_ready_ = false;
        }
    }

    void Ps2Mouse::process_packet() {
        // If either overflow flag is set the delta values are garbage, discard.
        if ((packet_[0] & PS2_X_OVERFLOW) || (packet_[0] & PS2_Y_OVERFLOW)) return;

        const bool x_negative = packet_[0] & PS2_X_SIGN;
        const bool y_negative = packet_[0] & PS2_Y_SIGN;

        // Sign-extend the 9-bit deltas to i32
        const i32 dx = x_negative ? (static_cast<i32>(packet_[1]) - 256) : static_cast<i32>(packet_[1]);
        const i32 dy = (y_negative
                            ? (256 - static_cast<i32>(packet_[2]))
                            : -static_cast<i32>(packet_[2])); // Y inverted for screen-space

        i32 new_x = static_cast<i32>(position_.x) + dx;
        i32 new_y = static_cast<i32>(position_.y) + dy;

        const i32 max_w = static_cast<i32>(DisplayManager::primary().drv->screen_width_px());
        const i32 max_h = static_cast<i32>(DisplayManager::primary().drv->screen_height_px());

        if (new_x < 0) new_x = 0;
        if (new_x >= max_w) new_x = max_w - 1;
        if (new_y < 0) new_y = 0;
        if (new_y >= max_h) new_y = max_h - 1;

        position_.x = static_cast<u32>(new_x);
        position_.y = static_cast<u32>(new_y);

        kernel::input::MouseButtonMask buttons = 0;
        if (packet_[0] & PS2_LEFT_BUTTON) buttons |= static_cast<u8>(kernel::input::MouseButton::LEFT);
        if (packet_[0] & PS2_RIGHT_BUTTON) buttons |= static_cast<u8>(kernel::input::MouseButton::RIGHT);
        if (packet_[0] & PS2_MIDDLE_BUTTON) buttons |= static_cast<u8>(kernel::input::MouseButton::MIDDLE);

        // packet_[3] is the wheel byte; zero when running in 3-byte mode
        const i8 wheel = (packet_size_ == 4) ? static_cast<i8>(packet_[3]) : 0;

        kernel::input::InputEvent ev{
            .device = kernel::input::InputDeviceType::MOUSE,
            .mouse = {
                .x = position_.x,
                .y = position_.y,
                .delta_x = dx,
                .delta_y = dy,
                .wheel_delta = wheel,
                .buttons_pressed = buttons,
            }
        };

        kernel::input::MiceDevice::share_mouse_event(ev);

        position_old_ = position_;
    }
} // namespace ps2::mouse
