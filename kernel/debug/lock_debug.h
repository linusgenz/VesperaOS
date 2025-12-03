// lock_debug.h
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

#ifndef VESPERAOS_LOCK_DEBUG_H
#define VESPERAOS_LOCK_DEBUG_H

#include <cstdint>
class Unit;

#define MAX_DEBUG_LOCKS 256
#define MAX_WAITERS_PER_LOCK 16
#define MAX_STACK_TRACE_DEPTH 32
#define MAX_LOCK_NAME_LEN 64

struct lock_debug_info {
    void* lock_ptr; // pointer to the spinlock object
    char name[MAX_LOCK_NAME_LEN];

    // owner unit id (0 == none)
    uint32_t owner_unit;

    // when was it acquired (rdtsc)
    uint64_t acquired_tsc;

    // waiters (unit ids)
    uint32_t waiters[MAX_WAITERS_PER_LOCK];
    uint8_t waiter_count;

    // last stack traces (for owner and first waiter)
    uint64_t owner_trace[MAX_STACK_TRACE_DEPTH];
    uint8_t owner_trace_len;

    uint64_t waiter_trace[MAX_STACK_TRACE_DEPTH];
    uint8_t waiter_trace_len;
};

void lock_debug_init();

lock_debug_info* lock_debug_register(void* lockptr, const char* name);

// called by instrumented lock code before attempting acquire
void lock_debug_before_acquire(const void* lockptr, uint32_t current_unit);


// called by instrumented lock code after successful acquire
void lock_debug_after_acquire(const void* lockptr, uint32_t current_unit);


// called by instrumented unlock
void lock_debug_release(const void* lockptr, uint32_t current_unit);


// run cycle detection and print a report if cycle found
bool lock_debug_detect_deadlocks_and_report();


void lock_debug_report_deadlock(lock_debug_info* L, uint32_t start_unit);

lock_debug_info* lock_debug_find(const void* lockptr);

#endif //VESPERAOS_LOCK_DEBUG_H