// reentrant_spinlock.h
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

#ifndef VESPERAOS_REENTRANT_SPINLOCK_H
#define VESPERAOS_REENTRANT_SPINLOCK_H

#include <vespera/types.h>

struct ReentrantSpinlock
{
    volatile u32 locked = 0; // atomic lock
    u32 owner_unit = 0; // Unit, die den Lock hält
    u32 recursion = 0; // Rekursionszähler

    void init()
    {
        locked = 0;
        owner_unit = 0;
        recursion = 0;
    }

    void lock();

    void unlock();

    void lock_irqsave(u64& flags)
    {
        flags = irq_save();
        lock();
    }

    void unlock_irqrestore(u64 flags)
    {
        unlock();
        irq_restore(flags);
    }

private:
    u32 xchg(volatile u32* ptr, u32 val)
    {
        u32 old = 0;
        __asm__ volatile (
            "lock xchg %0, %1"
            : "=r"(old), "+m"(*ptr)
            : "0"(val)
            : "memory"
        );
        return old;
    }

    u64 irq_save()
    {
        u64 flags = 0;
        asm volatile(
            "pushfq\n\t"
            "popq %0\n\t"
            "cli"
            : "=r"(flags)
            :
            : "memory"
        );
        return flags;
    }

    void irq_restore(u64 flags)
    {
        asm volatile(
            "pushq %0\n\t"
            "popfq"
            :
            : "r"(flags)
            : "memory", "cc"
        );
    }
};

struct ReentrantSpinlockGuard
{
    ReentrantSpinlock& lock_ref;

    explicit ReentrantSpinlockGuard(ReentrantSpinlock& lock) : lock_ref(lock)
    {
        lock_ref.lock();
    }

    ~ReentrantSpinlockGuard()
    {
        lock_ref.unlock();
    }

    ReentrantSpinlockGuard(const ReentrantSpinlockGuard&) = delete;
    ReentrantSpinlockGuard& operator=(const ReentrantSpinlockGuard&) = delete;
};

struct ReentrantSpinlockGuardIrq
{
    ReentrantSpinlock& lock_ref;
    u64 flags{};

    explicit ReentrantSpinlockGuardIrq(ReentrantSpinlock& lock) : lock_ref(lock)
    {
        lock_ref.lock_irqsave(flags);
    }

    ~ReentrantSpinlockGuardIrq()
    {
        lock_ref.unlock_irqrestore(flags);
    }

    ReentrantSpinlockGuardIrq(const ReentrantSpinlockGuardIrq&) = delete;
    ReentrantSpinlockGuardIrq& operator=(const ReentrantSpinlockGuardIrq&) = delete;
};

#endif //VESPERAOS_REENTRANT_SPINLOCK_H
