// spinlock.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 20.11.25.
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

#include <kernel/sync/spinlock.h>

#include <kernel/scheduling.h>

#include <kernel/system/system_manager.h>

#if DEBUG_SPINLOCK
#include "../../kernel/debug/lock_debug.h"
#endif

void spinlock_t::init(const char* name)
{
    locked = 0;
#if DEBUG_SPINLOCK
    lock_debug_register(this, name);
#endif
}

void spinlock_t::lock()
{
    uint32_t uid = kernel::scheduling::get_current_unit()->id;

    //  auto* info = lock_debug_find(this);

    /* if (info && uid != 0 && info->owner_unit == uid) {
         Log::PrintLn("*** SELF-DEADLOCK: unit %u tried to relock %s", uid, info->name);
         lock_debug_report_deadlock(info, uid);
         kernel::SystemManager::system_panic("SELF-DEADLOCK", -KESELFDEADLK);
     }*/

    //  lock_debug_before_acquire(this, uid);

    while (xchg(&locked, 1))
    {
        //  lock_debug_before_acquire(this, uid);
        asm volatile("pause");
    }

    // lock_debug_after_acquire(this, uid);
}

void spinlock_t::unlock()
{
    uint32_t uid = kernel::scheduling::get_current_unit()->id;

    //  lock_debug_release(this, uid);

    __atomic_store_n(&locked, 0, __ATOMIC_RELEASE);
}
