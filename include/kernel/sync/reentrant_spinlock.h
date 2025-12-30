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

#include <cstdint>

struct reentrant_spinlock_t
{
    volatile uint32_t locked = 0; // atomic lock
    uint32_t owner_unit = 0; // Unit, die den Lock hält
    uint32_t recursion = 0; // Rekursionszähler

    void init()
    {
        locked = 0;
        owner_unit = 0;
        recursion = 0;
    }

    void lock();

    void unlock();

    void lock_irqsave(uint64_t& flags)
    {
        flags = irq_save();
        lock();
    }

    void unlock_irqrestore(uint64_t flags)
    {
        unlock();
        irq_restore(flags);
    }

private:
    uint32_t xchg(volatile uint32_t* ptr, uint32_t val)
    {
        uint32_t old;
        __asm__ volatile (
            "lock xchg %0, %1"
            : "=r"(old), "+m"(*ptr)
            : "0"(val)
            : "memory"
        );
        return old;
    }

    uint64_t irq_save()
    {
        uint64_t flags;
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

    void irq_restore(uint64_t flags)
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

struct reentrant_spinlock_guard
{
    reentrant_spinlock_t& lock_ref;

    explicit reentrant_spinlock_guard(reentrant_spinlock_t& lock) : lock_ref(lock)
    {
        lock_ref.lock();
    }

    ~reentrant_spinlock_guard()
    {
        lock_ref.unlock();
    }

    reentrant_spinlock_guard(const reentrant_spinlock_guard&) = delete;
    reentrant_spinlock_guard& operator=(const reentrant_spinlock_guard&) = delete;
};

struct reentrant_spinlock_guard_irq
{
    reentrant_spinlock_t& lock_ref;
    uint64_t flags{};

    explicit reentrant_spinlock_guard_irq(reentrant_spinlock_t& lock) : lock_ref(lock)
    {
        lock_ref.lock_irqsave(flags);
    }

    ~reentrant_spinlock_guard_irq()
    {
        lock_ref.unlock_irqrestore(flags);
    }

    reentrant_spinlock_guard_irq(const reentrant_spinlock_guard_irq&) = delete;
    reentrant_spinlock_guard_irq& operator=(const reentrant_spinlock_guard_irq&) = delete;
};

#endif //VESPERAOS_REENTRANT_SPINLOCK_H
