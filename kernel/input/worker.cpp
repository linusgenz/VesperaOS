// worker.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 15.11.25.
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

#include <vespera/graphics/colors.h>
#include <vespera/input/input_manager.h>
#include <vespera/realm/realm_types.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>
#include <vespera/tty/tty.h>
#include <vespera/unit_config.h>

#include "vespera/unit/unit_manager.h"

constexpr u8 CURSOR_BITMAP[] = {
    0b10000000, 0b00000000, 0b11000000, 0b00000000, 0b11100000, 0b00000000, 0b11110000, 0b00000000,
    0b11111000, 0b00000000, 0b11111100, 0b00000000, 0b11111110, 0b00000000, 0b11111111, 0b00000000,
    0b11111111, 0b10000000, 0b11111111, 0b11000000, 0b11111111, 0b11100000, 0b11111111, 0b10000000,
    0b11111111, 0b00000000, 0b11000111, 0b00000000, 0b00000011, 0b00000000, 0b00000001, 0b00000000,
};

[[noreturn]] void input_poll_thread(void* arg) {
    kernel::input::InputEvent ev{};
    while (true) {
        while (kernel::input::InputManager::pop_event(ev)) {
            switch (ev.device) {
                case kernel::input::InputDeviceType::KEYBOARD: {
                    kernel::tty::tty_handle_input(ev.key);
                    break;
                }
                default:
                    break;
            }
        }
        kernel::scheduling::yield();
    }
}

void initialize_input_bus() {
    constexpr UnitConfig uc = {
        .name = "input_bus",
        .cpu_id = 3,
        .priority = 5,
        .stack_size = DEFAULT_UNIT_STACK_SIZE,
        .initial_handles = nullptr,
        .initial_handle_count = 0,
        .is_idle = false,
        .is_user = false,
        .user_stack_size = 0
    };
    UnitManager::create(kernel::realm::REALM_SYSTEM, input_poll_thread, nullptr, &uc);
}