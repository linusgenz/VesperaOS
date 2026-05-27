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

#include <drivers/ps2/mouse.h>  // Der saubere öffentliche Header
#include <vespera/cpu/io.h>
#include <vespera/graphics/colors.h>
#include <vespera/graphics/display_manager.h>
#include <vespera/input/input_manager.h>
#include <vespera/system/system_manager.h>

#include "vespera/kernel_utils.h"

// ANONYMER NAMESPACE: Alles hier drin ist komplett isoliert in dieser Datei!
namespace {
    // Bitmasks als constexpr
    constexpr u8 PS2_LEFT_BUTTON = 0b00000001;
    constexpr u8 PS2_RIGHT_BUTTON = 0b00000010;
    constexpr u8 PS2_MIDDLE_BUTTON = 0b00000100;
    constexpr u8 PS2_ALWAYS_ONE = 0b00001000;
    constexpr u8 PS2_X_SIGN = 0b00010000;
    constexpr u8 PS2_Y_SIGN = 0b00100000;
    constexpr u8 PS2_X_OVERFLOW = 0b01000000;
    constexpr u8 PS2_Y_OVERFLOW = 0b10000000;

    // Port-Adressen
    constexpr u16 PS2_DATA_PORT = 0x60;
    constexpr u16 PS2_CMD_PORT = 0x64;

    // Rein hardwarenahe Hilfsfunktionen, die die Klasse nicht exposen muss
    void wait_write() {
        for (int i = 0; i < 100000; ++i) {
            if ((inb(PS2_CMD_PORT) & 0x2) == 0) return;
        }
    }

    void wait_read() {
        for (int i = 0; i < 100000; ++i) {
            if ((inb(PS2_CMD_PORT) & 0x1) == 1) return;
        }
    }

    void send_cmd(const u8 byte) {
        wait_write();
        outb(PS2_CMD_PORT, 0xD4);
        wait_write();
        outb(PS2_DATA_PORT, byte);
    }

    u8 recv_response() {
        wait_read();
        return inb(PS2_DATA_PORT);
    }

    constexpr u8 CURSOR_BITMAP[] = {
        0b10000000, 0b00000000, 0b11000000, 0b00000000, 0b11100000, 0b00000000, 0b11110000, 0b00000000,
        0b11111000, 0b00000000, 0b11111100, 0b00000000, 0b11111110, 0b00000000, 0b11111111, 0b00000000,
        0b11111111, 0b10000000, 0b11111111, 0b11000000, 0b11111111, 0b11100000, 0b11111111, 0b10000000,
        0b11111111, 0b00000000, 0b11000111, 0b00000000, 0b00000011, 0b00000000, 0b00000001, 0b00000000,
    };
}  // namespace

namespace ps2::mouse {

    point_t Ps2Mouse::position_{0, 0};
    point_t Ps2Mouse::position_old_{0, 0};
    u8 Ps2Mouse::packet_[4]{0};
    u8 Ps2Mouse::cycle_{0};
    bool Ps2Mouse::packet_ready_{false};
    bool Ps2Mouse::first_byte_skipped_{false};

    point_t Ps2Mouse::get_position() {
        return position_;
    }

    void Ps2Mouse::init() {
        outb(PS2_CMD_PORT, 0xA8);

        wait_write();
        outb(PS2_CMD_PORT, 0x20);
        wait_read();

        u8 status = inb(PS2_DATA_PORT);
        status |= 0b10;

        wait_write();
        outb(PS2_CMD_PORT, 0x60);
        wait_write();
        outb(PS2_DATA_PORT, status);

        send_cmd(0xF6);
        recv_response();

        send_cmd(0xF4);
        recv_response();

        // IntelliMouse Modus aktivieren
        send_cmd(0xF3);
        send_cmd(200);
        send_cmd(0xF3);
        send_cmd(100);
        send_cmd(0xF3);
        send_cmd(80);
    }

    void Ps2Mouse::handle_byte(const u8 data) {
        if (!first_byte_skipped_) {
            first_byte_skipped_ = true;
            return;
        }

        switch (cycle_) {
            case 0:
                if ((data & PS2_ALWAYS_ONE) == 0) break;
                packet_[0] = data;
                cycle_++;
                break;
            case 1:
                packet_[1] = data;
                cycle_++;
                break;
            case 2:
                packet_[2] = data;
                cycle_++;
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
        const bool x_negative = packet_[0] & PS2_X_SIGN;
        const bool y_negative = packet_[0] & PS2_Y_SIGN;
        const bool x_overflow = packet_[0] & PS2_X_OVERFLOW;
        const bool y_overflow = packet_[0] & PS2_Y_OVERFLOW;

        i32 dx = x_negative ? (static_cast<i32>(packet_[1]) - 256) : packet_[1];
        i32 dy = y_negative ? (256 - static_cast<i32>(packet_[2]))
                            : -static_cast<i32>(packet_[2]);  // Y-Achse invertiert für Screen-Space

        if (x_overflow) dx += (dx > 0) ? 255 : -255;
        if (y_overflow) dy += (dy > 0) ? 255 : -255;

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

        const i8 wheel = static_cast<i8>(packet_[3]);

        kernel::input::InputEvent ev{
            .device = kernel::input::InputDeviceType::MOUSE,
            .mouse = {
                      .x = position_.x,
                      .y = position_.y,
                      .delta_x = dx,
                      .delta_y = dy,
                      .wheel_delta = wheel,
                      .buttons_pressed = buttons
            }
        };

        kernel::input::InputManager::push_event(ev);

        auto* term = kernel::SystemManager::get_system_terminal();
        if (term) {
            term->clear_mouse_cursor(CURSOR_BITMAP, position_old_);
            term->draw_overlay_mouse_cursor(CURSOR_BITMAP, position_, WHITE);
        }

        position_old_ = position_;
    }
}  // namespace ps2::mouse