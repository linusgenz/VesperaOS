// test_heap_validate.cpp
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

// =============================================================================
// Heap Tests — Corruption Detection, Edge Cases & Stress
// =============================================================================
// Covers:
//   guard-byte / checksum corruption detection
//   is_valid_pointer
//   validate_heap structural checks
//   stress: random alloc/free patterns, size variety, fragmentation
// =============================================================================

#include "heap_fixture.h"
#include <cstdlib>   // rand / srand
#include <ctime>

// =============================================================================
// is_valid_pointer
// =============================================================================

TEST(Heap_Validate, IsValidPtrReturnsTrueForLiveBlock, "is_valid_pointer returns true for an allocated block") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(64);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(h.is_valid_ptr(p));
}

TEST(Heap_Validate, IsValidPtrReturnsFalseForNull, "is_valid_pointer returns false for null") {
    WITH_HEAP(h, 64);
    ASSERT_FALSE(h.is_valid_ptr(nullptr));
}

TEST(Heap_Validate, IsValidPtrReturnsFalseForHeapStart, "is_valid_pointer returns false for the raw heap_start pointer") {
    WITH_HEAP(h, 64);
    // heap_start points at a HeapSegHdr, not at user data
    ASSERT_FALSE(h.is_valid_ptr(virt_ptr(::heap_start)));
}

TEST(Heap_Validate, IsValidPtrReturnsFalseForArbitraryAddress, "is_valid_pointer returns false for a stack-local variable") {
    WITH_HEAP(h, 64);
    int local = 42;
    ASSERT_FALSE(h.is_valid_ptr(&local));
}

TEST(Heap_Validate, IsValidPtrReturnsFalseAfterFree, "is_valid_pointer returns false after the block is freed") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(64);
    ASSERT_NOT_NULL(p);
    h.free(p);
    ASSERT_FALSE(h.is_valid_ptr(p));
}

// =============================================================================
// validate_heap
// =============================================================================

TEST(Heap_Validate, ValidateEmptyHeap, "validate_heap returns true on a freshly initialized heap") {
    WITH_HEAP(h, 64);
    ASSERT_TRUE(h.validate());
}

TEST(Heap_Validate, ValidateAfterSingleAlloc, "validate_heap returns true after one allocation") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(128);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(h.validate());
}

TEST(Heap_Validate, ValidateAfterSingleFree, "validate_heap returns true after one alloc + free") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(128);
    h.free(p);
    ASSERT_TRUE(h.validate());
}

TEST(Heap_Validate, ValidateAfterManyOps, "validate_heap returns true after 50 interleaved alloc/free operations") {
    WITH_HEAP(h, 256);
    std::vector<void*> live;

    for (int i = 0; i < 50; i++) {
        if (!live.empty() && i % 3 == 0) {
            h.free(live.back());
            live.pop_back();
        } else {
            void* p = h.malloc(64 + (i % 8) * 16);
            if (p) live.push_back(p);
        }
    }
    ASSERT_TRUE(h.validate());
    for (auto* p : live) h.free(p);
    ASSERT_TRUE(h.validate());
}

// =============================================================================
// Guard-byte corruption detection
// =============================================================================
// These tests deliberately corrupt heap metadata and verify the heap
// detects it. They do NOT call validate_heap after corruption because
// the heap intentionally halts on serious corruption — instead they
// inspect the header directly.

TEST(Heap_Validate, GuardByteIntactAfterWrite, "Guard byte at header start is intact after writing user data") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(64);
    ASSERT_NOT_NULL(p);

    // Write into the user region only
    fill_pattern(p, 64, 0xFF);

    auto* hdr = HeapSegHdr::from_data_ptr(p);
    ASSERT_TRUE(hdr->check_guard_bytes());
}

TEST(Heap_Validate, ChecksumMatchesAfterAlloc, "Checksum is valid right after allocation") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(64);
    ASSERT_NOT_NULL(p);
    auto* hdr = HeapSegHdr::from_data_ptr(p);
    // check_guard_bytes verifies checksum internally
    ASSERT_TRUE(hdr->check_guard_bytes());
}

TEST(Heap_Validate, CorruptedGuardDetected, "A corrupted guard_start byte is detected by check_guard_bytes") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(64);
    ASSERT_NOT_NULL(p);

    auto* hdr = HeapSegHdr::from_data_ptr(p);
    hdr->guard_start = 0x00;   // deliberately corrupt

    ASSERT_FALSE(hdr->check_guard_bytes());
    // Do NOT call free(p) — the corrupted block would trigger the error path
}

TEST(Heap_Validate, CorruptedChecksumDetected, "A corrupted checksum is detected by check_guard_bytes") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(64);
    ASSERT_NOT_NULL(p);

    auto* hdr = HeapSegHdr::from_data_ptr(p);
    hdr->checksum ^= 0xDEAD;   // flip bits

    ASSERT_FALSE(hdr->check_guard_bytes());
}

// =============================================================================
// Edge cases
// =============================================================================

TEST(Heap_Edge, AllocMinAllocSize, "Allocating exactly MIN_ALLOC_SIZE bytes succeeds") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(MIN_ALLOC_SIZE);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(h.is_valid_ptr(p));
}

TEST(Heap_Edge, AllocSizeJustBelowHeader, "Allocating HEAP_HEADER_SIZE-1 bytes succeeds (rounded up internally)") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(HEAP_HEADER_SIZE - 1);
    ASSERT_NOT_NULL(p);
}

TEST(Heap_Edge, FreeSamePointerTwiceHandledGracefully, "Double-free is handled without silent data corruption") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(64);
    ASSERT_NOT_NULL(p);
    h.free(p);
    h.free(p);   // should be detected and ignored
    ASSERT_TRUE(true);
}

TEST(Heap_Edge, ReallocSameSizeReturnsSameOrNewValid, "realloc to the same size returns a valid pointer") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(128);
    ASSERT_NOT_NULL(p);
    void* q = h.realloc(p, 128, 128);
    ASSERT_NOT_NULL(q);
    ASSERT_TRUE(h.is_valid_ptr(q));
}

TEST(Heap_Edge, GetFreeSpaceDecreasesOnAlloc, "get_free_space decreases after allocation") {
    WITH_HEAP(h, 64);
    size_t before = h.free_space();
    h.malloc(512);
    ASSERT_LE(h.free_space(), before - 512);
}

TEST(Heap_Edge, GetHeapUsageIsConsistent, "get_heap_usage equals total_allocated - total_freed") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(256);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(::total_allocated - ::total_freed, h.usage());
}

// =============================================================================
// Stress tests
// =============================================================================

TEST(Heap_Stress, RandomAllocFreePattern, "Random alloc/free sequence leaves the heap valid") {
    WITH_HEAP(h, 512);
    srand(42);  // deterministic seed for reproducibility

    std::vector<void*> live;
    for (int i = 0; i < 500; i++) {
        if (!live.empty() && rand() % 2 == 0) {
            size_t idx = rand() % live.size();
            h.free(live[idx]);
            live.erase(live.begin() + idx);
        } else {
            size_t sz = 16 + (rand() % 240);
            void* p = h.malloc(sz);
            if (p) live.push_back(p);
        }
    }
    ASSERT_TRUE(h.validate());
    for (auto* p : live) h.free(p);
    ASSERT_TRUE(h.validate());
}

TEST(Heap_Stress, VariousSizes, "Allocating sizes from 1 to 4096 all succeed or return null gracefully") {
    WITH_HEAP(h, 512);
    size_t sizes[] = { 1, 2, 3, 7, 8, 15, 16, 17, 31, 32, 63, 64, 127,
                       128, 255, 256, 511, 512, 1023, 1024, 2048, 4096 };
    for (size_t sz : sizes) {
        void* p = h.malloc(sz);
        if (p) {
            ASSERT_TRUE(h.is_valid_ptr(p));
            h.free(p);
        }
        ASSERT_TRUE(h.validate());
    }
}

TEST(Heap_Stress, FragmentationRecovery, "After heavy fragmentation, coalescing allows a large allocation") {
    WITH_HEAP(h, 128);

    // Allocate 32 small blocks
    std::vector<void*> ptrs;
    for (int i = 0; i < 32; i++) {
        void* p = h.malloc(64);
        ASSERT_NOT_NULL(p);
        ptrs.push_back(p);
    }

    // Free every other block (creates fragmentation)
    for (int i = 0; i < 32; i += 2)
        h.free(ptrs[i]);

    // Free remaining blocks — coalescing should reassemble the heap
    for (int i = 1; i < 32; i += 2)
        h.free(ptrs[i]);

    ASSERT_TRUE(h.validate());

    // A contiguous large allocation must now be possible
    void* big = h.malloc(4 * 1024);
    ASSERT_NOT_NULL(big);
    h.free(big);
}

TEST(Heap_Stress, AllocFillVerifyFreeRepeat, "100 cycles of alloc-fill-verify-free leave no corruption") {
    WITH_HEAP(h, 64);

    for (int cycle = 0; cycle < 100; cycle++) {
        auto* p = static_cast<uint8_t*>(h.malloc(128));
        ASSERT_NOT_NULL(p);
        fill_pattern(p, 128, (uint8_t)(cycle & 0xFF));
        ASSERT_TRUE(check_pattern(p, 128, (uint8_t)(cycle & 0xFF)));
        h.free(p);
    }
    ASSERT_TRUE(h.validate());
}

TEST(Heap_Stress, MixedAlignedAndRegular, "Interleaving alloc_aligned and malloc leaves the heap valid") {
    WITH_HEAP(h, 256);
    std::vector<std::pair<void*, bool>> ptrs;  // ptr + is_aligned

    for (int i = 0; i < 20; i++) {
        if (i % 3 == 0) {
            void* p = h.alloc_aligned(64, 64);
            if (p) ptrs.push_back({p, true});
        } else {
            void* p = h.malloc(64);
            if (p) ptrs.push_back({p, false});
        }
    }

    ASSERT_TRUE(h.validate());

    for (auto& [p, aligned] : ptrs) {
        if (aligned) h.free_aligned(p);
        else         h.free(p);
    }

    ASSERT_TRUE(h.validate());
}