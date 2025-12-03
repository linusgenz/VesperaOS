// lock_debug.cpp
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

#include "lock_debug.h"

#include <log.h>
#include <kernel/memory.h>
#include <string.h>

#define LOCK_HELD_TIMEOUT_CYCLES (100ULL * 1000ULL * 1000ULL * 1000ULL * 10000ULL)

static lock_debug_info lock_table[MAX_DEBUG_LOCKS];
static uint32_t lock_table_count = 0;

lock_debug_info* lock_debug_find(const void* lockptr) {
    for (uint32_t i = 0; i < lock_table_count; ++i) {
        if (lock_table[i].lock_ptr == lockptr) return &lock_table[i];
    }
    return nullptr;
}

void lock_debug_init() {
    memset(lock_table, 0, sizeof(lock_table));
    lock_table_count = 0;
}

lock_debug_info* lock_debug_register(void* lockptr, const char* name) {
    if (!lockptr) return nullptr;
    if (auto *e = lock_debug_find(lockptr)) return e;
    if (lock_table_count >= MAX_DEBUG_LOCKS) return nullptr;
    lock_debug_info &inf = lock_table[lock_table_count++];
    inf.lock_ptr = lockptr;
    strncpy(inf.name, name ? name : "<unnamed>", MAX_LOCK_NAME_LEN - 1);
    inf.owner_unit = 0;
    inf.acquired_tsc = 0;
    inf.waiter_count = 0;
    inf.owner_trace_len = 0;
    inf.waiter_trace_len = 0;
    return &inf;
}

static uint8_t capture_stack_trace(uint64_t* out_buf, uint8_t max_depth) {
    uint64_t *rbp;
    asm volatile ("mov %%rbp, %0" : "=r" (rbp));
    uint8_t cnt = 0;
    while (rbp && cnt < max_depth) {
        uint64_t ret = *(rbp + 1);
        if (!ret) break;
        out_buf[cnt++] = ret;
        rbp = reinterpret_cast<uint64_t*>(*rbp);
    }
    return cnt;
}

void lock_debug_before_acquire(const void* lockptr, const uint32_t current_unit) {
    if (current_unit == 0) return;
    if (!lockptr) return;
    if (auto *e = lock_debug_find(lockptr)) {
        // if lock has owner and owner != current, register current as waiter
        uint32_t owner = e->owner_unit;
        if (owner != 0 && owner != current_unit) {
            if (e->waiter_count < MAX_WAITERS_PER_LOCK) {
                e->waiters[e->waiter_count++] = current_unit;
            }
            // capture waiter stack trace (first waiter only)
            if (e->waiter_trace_len == 0) {
                e->waiter_trace_len = capture_stack_trace(e->waiter_trace, MAX_STACK_TRACE_DEPTH);
            }
        }
    }
}

inline uint64_t rdtsc() {
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}


void lock_debug_after_acquire(const void* lockptr, const uint32_t current_unit) {
    if (current_unit == 0) return;
    if (!lockptr) return;
    if (auto *e = lock_debug_find(lockptr)) {
        e->owner_unit = current_unit;
        e->acquired_tsc = rdtsc();
        // remove current_unit from waiters (if present)
        for (uint8_t i = 0; i < e->waiter_count; ++i) {
            if (e->waiters[i] == current_unit) {
                // shift
                for (uint8_t j = i; j + 1 < e->waiter_count; ++j) e->waiters[j] = e->waiters[j+1];
                e->waiter_count--;
                break;
            }
        }
        // capture owner trace
        e->owner_trace_len = capture_stack_trace(e->owner_trace, MAX_STACK_TRACE_DEPTH);
    }
}

void lock_debug_release(const void* lockptr, const uint32_t current_unit) {
    if (current_unit == 0) return;
    if (!lockptr) return;
    if (auto *e = lock_debug_find(lockptr)) {
        if (e->owner_unit == current_unit) {
            e->owner_unit = 0;
            e->acquired_tsc = 0;
            e->owner_trace_len = 0;
            // clear waiter trace so next waiter captures fresh
            e->waiter_trace_len = 0;
            e->waiter_count = 0; // conservative: we expect waiters to re-register next time
        }
    }
}

static bool dfs_detect(uint32_t start_unit, uint32_t cur_unit, uint8_t* visited, uint8_t depth) {
    if (depth > 128) return false; // safety
    // current unit is cur_unit, check what it waits for
    // find lock where cur_unit is waiter (first such lock), then follow to owner
    for (uint32_t i = 0; i < lock_table_count; ++i) {
        lock_debug_info &L = lock_table[i];
        for (uint8_t w = 0; w < L.waiter_count; ++w) {
            if (L.waiters[w] == cur_unit) {
                uint32_t owner = L.owner_unit;
                if (owner == 0) continue;
                if (owner == start_unit) {
                    // cycle
                    return true;
                }
                if (visited[owner]) continue;
                visited[owner] = 1;
                if (dfs_detect(start_unit, owner, visited, depth+1)) return true;
            }
        }
    }
    return false;
}


bool lock_debug_detect_deadlocks_and_report() {
    static bool deadlock_reported = false;
    if (deadlock_reported) return true;

    uint8_t visited[1024];

    for (uint32_t i = 0; i < lock_table_count; ++i) {
        lock_debug_info &L = lock_table[i];
        for (uint8_t w = 0; w < L.waiter_count; ++w) {
            uint32_t start = L.waiters[w];
            memset(visited, 0, sizeof(visited));
            visited[start] = 1;

            if (dfs_detect(start, start, visited, 0)) {
                deadlock_reported = true;
                lock_debug_report_deadlock(&L, start);
                return true; // nur 1 Meldung
            }
        }
    }

   /* uint64_t now = rdtsc();

    for (uint32_t i = 0; i < lock_table_count; ++i) {
        lock_debug_info &L = lock_table[i];
        if (L.owner_unit != 0 && L.acquired_tsc != 0) {
            uint64_t held = now - L.acquired_tsc;
            if (held >= LOCK_HELD_TIMEOUT_CYCLES) {
                deadlock_reported = true;
                Log::PrintLn("*** LOCK-TIMEOUT: lock %s held too long (owner=%u)", L.name, L.owner_unit);
                // Report for this lock: owner + waiter trace if any
                lock_debug_report_deadlock(&L, L.waiters[0]); // pass first waiter (if exist) as 'start'
                return true;
            }
        }
    }*/

    return false;
}


void lock_debug_report_deadlock(lock_debug_info* L, uint32_t start_unit) {
    Log::PrintLn("=== DEADLOCK DETECTED ===");
    Log::PrintLn("Unit %u waits on lock: %s (%p)", start_unit, L->name, L->lock_ptr);

    // Owner
    if (L->owner_unit) {
        Log::PrintLn("Current owner: %u", L->owner_unit);
        if (L->owner_trace_len) {
            Log::PrintLn("Owner stack (%u frames):", L->owner_trace_len);
            for (uint8_t i = 0; i < L->owner_trace_len; ++i)
                Log::PrintLn(" %p", reinterpret_cast<void*>(L->owner_trace[i]));
        }
    }

    // Waiter trace
    if (L->waiter_trace_len) {
        Log::PrintLn("Waiter stack (%u frames):", L->waiter_trace_len);
        for (uint8_t i = 0; i < L->waiter_trace_len; ++i)
            Log::PrintLn(" %p", reinterpret_cast<void*>(L->waiter_trace[i]));
    }

    // Optional: wie lange Lock gehalten wird
    if (L->acquired_tsc)
        Log::PrintLn("Lock held for: %llu cycles", rdtsc() - L->acquired_tsc);
}