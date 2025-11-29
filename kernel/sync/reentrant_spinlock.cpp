// reentrant_spinlock.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 21.11.25.
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

#include <kernel/sync/reentrant_spinlock.h>

#include <cstdint>
#include <kernel/scheduling.h>

void reentrant_spinlock_t::lock() {

    uint32_t uid = kernel::scheduling::get_current_unit()->id;

    if (owner_unit == uid) {
        recursion++;
        return;
    }

    // normalen Spinlock nehmen
    while (xchg(&locked, 1)) {
        asm volatile("pause");
    }

    owner_unit = uid;
    recursion = 1;
}

void reentrant_spinlock_t::unlock() {
    if (--recursion == 0) {
        owner_unit = 0;
        __atomic_store_n(&locked, 0, __ATOMIC_RELEASE);
    }
}