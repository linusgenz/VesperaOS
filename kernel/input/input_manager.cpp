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

#include <kernel/input/input_event.h>
#include <kernel/input/input_manager.h>
#include <kernel/memory.h>
#include <log.h>

namespace kernel::input {

    void InputManager::init() {
        s_lock_.init();
        s_head_ = 0;
        s_tail_ = 0;
        memset(s_buffer_, 0, sizeof(s_buffer_));
    }

    void InputManager::push_event(const InputEvent& ev) {
        SpinlockGuardIrq g(s_lock_);
        if (const size_t next = (s_head_ + 1) % BUFFER_SIZE; next != s_tail_) {
            s_buffer_[s_head_] = ev;
            s_head_ = next;
        }
    }

    bool InputManager::pop_event(InputEvent& ev) {
        SpinlockGuardIrq g(s_lock_);
        if (s_head_ == s_tail_) return false;
        ev = s_buffer_[s_tail_];
        s_tail_ = (s_tail_ + 1) % BUFFER_SIZE;
        return true;
    }

    bool InputManager::is_empty() {
        SpinlockGuard g(s_lock_);
        return s_head_ == s_tail_;
    }

}  // namespace kernel::input
