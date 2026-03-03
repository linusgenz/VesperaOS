// heap_fixture.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 03.03.26.
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

#ifndef VESPERAOS_HEAP_FIXTURE_H
#define VESPERAOS_HEAP_FIXTURE_H

#include "../../kernel/memory/heap.h"
#include "../framework/test_framework.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// =============================================================================
// HeapArena
// =============================================================================

struct HeapArena {
    // Saved global heap state (restored in destructor)
    void*       saved_heap_start  = nullptr;
    void*       saved_heap_end    = nullptr;
    HeapSegHdr* saved_last_hdr    = nullptr;
    bool        saved_initialized = false;
    size_t      saved_allocated   = 0;
    size_t      saved_freed       = 0;
    size_t      saved_peak        = 0;

    // Arena backing buffer
    uint8_t*    arena      = nullptr;
    size_t      arena_size = 0;

    // -------------------------------------------------------------------------
    // Constructor / Destructor
    // -------------------------------------------------------------------------

    explicit HeapArena(size_t kb = 64) {
        arena_size = kb * 1024;

        // Page-aligned so our in-place HeapSegHdr writes cannot touch
        // glibc's bookkeeping bytes that sit before a plain malloc() block.
        void* raw = nullptr;
        if (posix_memalign(&raw, 4096, arena_size) != 0) return;
        arena = static_cast<uint8_t*>(raw);
        std::memset(arena, 0, arena_size);

        // Save globals so sequential tests do not interfere.
        saved_heap_start  = ::heap_start;
        saved_heap_end    = ::heap_end;
        saved_last_hdr    = ::last_hdr;
        saved_initialized = ::heap_initialized;
        saved_allocated   = ::total_allocated;
        saved_freed       = ::total_freed;
        saved_peak        = ::peak_usage;

        ::heap_initialized = false;
        install();
    }

    ~HeapArena() {
        // Restore globals BEFORE freeing the buffer.
        ::heap_start       = saved_heap_start;
        ::heap_end         = saved_heap_end;
        ::last_hdr         = saved_last_hdr;
        ::heap_initialized = saved_initialized;
        ::total_allocated  = saved_allocated;
        ::total_freed      = saved_freed;
        ::peak_usage       = saved_peak;

        std::free(arena);
    }

    bool valid() const { return arena != nullptr && ::heap_initialized; }

    // -------------------------------------------------------------------------
    // Wrappers — call the renamed kernel functions, never glibc
    // -------------------------------------------------------------------------

    void*  malloc(size_t sz)                                   { return ::kmalloc(sz); }
    void   free(void* p)                                       { ::kfree(p); }
    void*  realloc(void* p, size_t old_sz, size_t new_sz)     { return ::krealloc(p, old_sz, new_sz); }
    void*  alloc_aligned(size_t sz, size_t al, size_t bd = 0) { return ::kalloc_aligned(sz, al, bd); }
    void   free_aligned(void* p)                               { ::kfree_aligned(p); }
    bool   validate()                                          { return ::validate_heap(); }
    bool   is_valid_ptr(void* p)                               { return ::is_valid_pointer(p); }
    size_t usage()                                             { return ::get_heap_usage(); }
    size_t free_space()                                        { return ::get_free_space(); }

private:
    void install() {
        uintptr_t buf_start = reinterpret_cast<uintptr_t>(arena);
        uintptr_t buf_end   = buf_start + arena_size;

        // Align the first header to MIN_ALIGNMENT.
        uintptr_t hdr_start = (buf_start + MIN_ALIGNMENT - 1) & ~(uintptr_t)(MIN_ALIGNMENT - 1);

        // Leave HEAP_HEADER_SIZE + 16 bytes of headroom at the end so that
        // expand_heap's blind write of a HeapSegHdr at heap_end stays
        // inside our arena rather than past it.
        const size_t tail_guard = HEAP_HEADER_SIZE + 16;
        uintptr_t heap_end_addr = buf_end - tail_guard;

        ::heap_start = reinterpret_cast<void*>(hdr_start);
        ::heap_end   = reinterpret_cast<void*>(heap_end_addr);

        size_t usable = heap_end_addr - hdr_start;
        if (usable <= HEAP_HEADER_SIZE) return;

        auto* hdr   = reinterpret_cast<HeapSegHdr*>(hdr_start);
        hdr->magic  = HEAP_MAGIC_FREE;
        hdr->length = usable - HEAP_HEADER_SIZE;
        hdr->next   = nullptr;
        hdr->last   = nullptr;
        hdr->free   = true;
        hdr->set_guard_bytes();

        ::last_hdr         = hdr;
        ::heap_initialized = true;
        ::total_allocated  = 0;
        ::total_freed      = 0;
        ::peak_usage       = 0;
    }
};

// =============================================================================
// WITH_HEAP(name, kb)
// =============================================================================

#define WITH_HEAP(var, kb) \
    HeapArena var(kb); \
    if (!(var).valid()) { \
        TestFramework::fail_test(__FILE__, __LINE__, \
            "HeapArena setup failed — posix_memalign returned error"); \
        return; \
    }

// =============================================================================
// Pattern helpers
// =============================================================================

inline void fill_pattern(void* ptr, size_t size, uint8_t pat) {
    std::memset(ptr, pat, size);
}

inline bool check_pattern(const void* ptr, size_t size, uint8_t pat) {
    const auto* p = static_cast<const uint8_t*>(ptr);
    for (size_t i = 0; i < size; i++)
        if (p[i] != pat) return false;
    return true;
}

#endif  // VESPERAOS_HEAP_FIXTURE_H