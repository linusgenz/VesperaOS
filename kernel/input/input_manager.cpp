// input_manager.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 09.09.25.
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

#include "input_manager.h"

#include <log.h>

namespace kernel::input {

    void InputManager::init() {
        s_head = 0;
        s_tail = 0;
        InputEvent s_buffer[BUFFER_SIZE] = {};
    }


    void InputManager::push_event(const InputEvent& ev) {
        size_t next = (s_head + 1) % BUFFER_SIZE;
        if (next != s_tail) {
            s_buffer[s_head] = ev;
            s_head = next;
        }
    }

    bool InputManager::pop_event(InputEvent& ev) {
        if (s_head == s_tail) return false;
        ev = s_buffer[s_tail];
        s_tail = (s_tail + 1) % BUFFER_SIZE;
        return true;
    }

    bool InputManager::is_empty() {
        return s_head == s_tail;
    }

}