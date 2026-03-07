// input_manager.h
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

#ifndef VESPERAOS_INPUT_MANAGER_H
#define VESPERAOS_INPUT_MANAGER_H

#include <stddef.h>
#include <vespera/input/input_event.h>
#include <vespera/sync/spinlock.h>

namespace kernel::input
{
    class InputManager
    {
    public:
        static constexpr size_t BUFFER_SIZE = 256;

        static void push_event(const InputEvent& ev);
        static bool pop_event(InputEvent& ev);
        static bool is_empty();
        static void init();

    private:
        static inline InputEvent s_buffer_[BUFFER_SIZE];
        static volatile inline size_t s_head_ = 0;
        static volatile inline size_t s_tail_ = 0;
        static inline Spinlock s_lock_{};
    };
}

#endif //VESPERAOS_INPUT_MANAGER_H
