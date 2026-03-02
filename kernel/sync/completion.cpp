// completion.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 04.12.25.
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

#include "../../include/kernel/sync/completion.h"

#include <kernel/sync/spinlock.h>
#include <kernel/time.h>

#include "../../include/log.h"

void completion_t::init() {
    completed = false;
    lock.init();
}

void completion_t::wait() const {
    while (!__atomic_load_n(&completed, __ATOMIC_ACQUIRE)) {
        kernel::time::sleep_ms(10);
    }
}

bool completion_t::wait_timeout(uint64_t timeout_ms) const {
    uint64_t start = kernel::time::get_ticks();
    while (!__atomic_load_n(&completed, __ATOMIC_ACQUIRE)) {
        if (const uint64_t elapsed = kernel::time::get_ticks() - start; elapsed > timeout_ms / 10) {  // ticks sind 10ms
            return false;
        }
        kernel::time::sleep_ms(10);
    }
    return true;
}

void completion_t::complete() {
    spinlock_guard guard(lock);
    __atomic_store_n(&completed, true, __ATOMIC_RELEASE);
}