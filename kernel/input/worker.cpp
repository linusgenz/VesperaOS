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

#include <memory.h>
#include <time.h>

#include "input_manager.h"
#include "../tty/tty.h"
#include "../units/unit.h"
#include "../units/unit_manager.h"


[[noreturn]] void input_poll_thread(void *arg) {
    kernel::input::InputEvent ev;
    memset(&ev, 0, sizeof(ev));
    while (true) {
        while (kernel::input::InputManager::pop_event(ev)) {
            kernel::tty::tty_handle_input(ev);
        }
        kernel::time::sleep_ms(10); // temp sync issues in input manager
        //  kernel::scheduling::yield(); // CPU an andere Threads
    }
}

void initialize_input_bus() {
    UnitConfig uc = {
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
    UnitManager::create(KERNEL_REALM_SYSTEM, (void *) input_poll_thread, nullptr, &uc);
}