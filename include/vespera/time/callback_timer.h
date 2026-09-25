// callback_timer.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 26.09.26.
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
#ifndef VESPERAOS_TIME_CALLBACK_TIMER_H
#define VESPERAOS_TIME_CALLBACK_TIMER_H

#include <vespera/types.h>

namespace kernel::time::callback_timer {
    using CallbackFn = void (*)(void*);

    struct CallbackHandle {
        u8  cpu_id      = 0xFF;
        u16 slot        = 0;
        u32 generation  = 0;

        [[nodiscard]] bool valid() const { return cpu_id != 0xFF; }
    };

    constexpr CallbackHandle INVALID_CALLBACK_HANDLE{};

    void init(u8 cpu_id);

    CallbackHandle schedule(u8 cpu_id, u64 deadline_ns, CallbackFn fn, void* arg);

    void cancel(CallbackHandle h);

    u64 earliest_deadline_ns(u8 cpu_id);

    void run_due_callbacks(u8 cpu_id);
}

#endif //VESPERAOS_TIME_CALLBACK_TIMER_H
