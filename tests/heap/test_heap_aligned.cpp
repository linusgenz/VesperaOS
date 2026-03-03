// test_heap_aligned.cpp
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
// Heap Tests — alloc_aligned / free_aligned
// =============================================================================
// Covers:
//   alloc_aligned - 16/32/64/4096-byte alignment, boundary constraints,
//                   data integrity, invalid arguments, free_aligned round-trip
// =============================================================================

#include "heap_fixture.h"

// Helper: check whether ptr is aligned to `align`
static bool is_aligned(const void* ptr, size_t align) {
    return (reinterpret_cast<uintptr_t>(ptr) % align) == 0;
}

// Helper: check whether [ptr, ptr+size) does NOT cross a `boundary`-byte boundary
static bool within_boundary(const void* ptr, size_t size, size_t boundary) {
    uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t end   = start + size - 1;
    return (start & ~(uintptr_t)(boundary - 1)) == (end & ~(uintptr_t)(boundary - 1));
}

// =============================================================================
// Basic alignment
// =============================================================================

TEST(Heap_Aligned, Align16, "alloc_aligned with alignment=16 returns a 16-byte-aligned pointer") {
    WITH_HEAP(h, 64);
    void* p = h.alloc_aligned(64, 16);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(is_aligned(p, 16));
    h.free_aligned(p);
}

TEST(Heap_Aligned, Align32, "alloc_aligned with alignment=32 returns a 32-byte-aligned pointer") {
    WITH_HEAP(h, 64);
    void* p = h.alloc_aligned(64, 32);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(is_aligned(p, 32));
    h.free_aligned(p);
}

TEST(Heap_Aligned, Align64, "alloc_aligned with alignment=64 returns a 64-byte-aligned pointer") {
    WITH_HEAP(h, 64);
    void* p = h.alloc_aligned(64, 64);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(is_aligned(p, 64));
    h.free_aligned(p);
}

TEST(Heap_Aligned, Align256, "alloc_aligned with alignment=256 returns a 256-byte-aligned pointer") {
    WITH_HEAP(h, 64);
    void* p = h.alloc_aligned(64, 256);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(is_aligned(p, 256));
    h.free_aligned(p);
}

TEST(Heap_Aligned, Align4096, "alloc_aligned with alignment=4096 (page-aligned) works") {
    WITH_HEAP(h, 256);
    void* p = h.alloc_aligned(128, 4096);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(is_aligned(p, 4096));
    h.free_aligned(p);
}

TEST(Heap_Aligned, MultipleAlignedAllocs, "Multiple alloc_aligned calls all satisfy the requested alignment") {
    WITH_HEAP(h, 256);
    const size_t alignments[] = { 16, 32, 64, 128, 256 };
    std::vector<void*> ptrs;

    for (size_t align : alignments) {
        void* p = h.alloc_aligned(64, align);
        ASSERT_NOT_NULL(p);
        ASSERT_TRUE(is_aligned(p, align));
        ptrs.push_back(p);
    }
    for (auto* p : ptrs) h.free_aligned(p);
    ASSERT_TRUE(h.validate());
}

// =============================================================================
// Data integrity
// =============================================================================

TEST(Heap_Aligned, DataIntegrity, "Data written to an aligned block survives a read-back") {
    WITH_HEAP(h, 64);
    auto* p = static_cast<uint8_t*>(h.alloc_aligned(256, 64));
    ASSERT_NOT_NULL(p);

    fill_pattern(p, 256, 0xDE);
    ASSERT_TRUE(check_pattern(p, 256, 0xDE));

    h.free_aligned(p);
}

TEST(Heap_Aligned, AlignedBlockDoesNotOverlapNeighbours, "Aligned block does not corrupt adjacent regular allocations") {
    WITH_HEAP(h, 64);
    auto* before = static_cast<uint8_t*>(h.malloc(64));
    auto* aligned = static_cast<uint8_t*>(h.alloc_aligned(128, 64));
    auto* after  = static_cast<uint8_t*>(h.malloc(64));
    ASSERT_NOT_NULL(before); ASSERT_NOT_NULL(aligned); ASSERT_NOT_NULL(after);

    fill_pattern(before,  64, 0x11);
    fill_pattern(aligned, 128, 0x22);
    fill_pattern(after,   64, 0x33);

    ASSERT_TRUE(check_pattern(before,  64, 0x11));
    ASSERT_TRUE(check_pattern(aligned, 128, 0x22));
    ASSERT_TRUE(check_pattern(after,   64, 0x33));

    h.free(before);
    h.free_aligned(aligned);
    h.free(after);
    ASSERT_TRUE(h.validate());
}

// =============================================================================
// Boundary constraints
// =============================================================================

TEST(Heap_Aligned, BoundaryConstraint64, "Allocation with boundary=64 does not cross a 64-byte boundary") {
    WITH_HEAP(h, 64);
    void* p = h.alloc_aligned(32, 16, 64);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(is_aligned(p, 16));
    ASSERT_TRUE(within_boundary(p, 32, 64));
    h.free_aligned(p);
}

TEST(Heap_Aligned, BoundaryConstraint4096, "Allocation with boundary=4096 stays within a single 4 KB page") {
    WITH_HEAP(h, 256);
    void* p = h.alloc_aligned(128, 16, 4096);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(within_boundary(p, 128, 4096));
    h.free_aligned(p);
}

TEST(Heap_Aligned, BoundaryAndAlignmentBothSatisfied, "Alignment and boundary constraints are both satisfied simultaneously") {
    WITH_HEAP(h, 256);
    void* p = h.alloc_aligned(64, 64, 256);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(is_aligned(p, 64));
    ASSERT_TRUE(within_boundary(p, 64, 256));
    h.free_aligned(p);
}

// =============================================================================
// Invalid arguments
// =============================================================================

TEST(Heap_Aligned, ZeroSizeReturnsNull, "alloc_aligned(0, ...) returns null") {
    WITH_HEAP(h, 64);
    ASSERT_NULL(h.alloc_aligned(0, 16));
}

TEST(Heap_Aligned, ZeroAlignmentReturnsNull, "alloc_aligned(..., 0) returns null") {
    WITH_HEAP(h, 64);
    ASSERT_NULL(h.alloc_aligned(64, 0));
}

TEST(Heap_Aligned, NonPowerOfTwoAlignmentReturnsNull, "alloc_aligned with non-power-of-two alignment returns null") {
    WITH_HEAP(h, 64);
    ASSERT_NULL(h.alloc_aligned(64, 3));
    ASSERT_NULL(h.alloc_aligned(64, 100));
}

TEST(Heap_Aligned, NonPowerOfTwoBoundaryReturnsNull, "alloc_aligned with non-power-of-two boundary returns null") {
    WITH_HEAP(h, 64);
    ASSERT_NULL(h.alloc_aligned(64, 16, 100));
}

// =============================================================================
// free_aligned
// =============================================================================

TEST(Heap_Aligned, FreeAlignedRestoresSpace, "free_aligned restores heap space") {
    WITH_HEAP(h, 64);
    size_t before = h.free_space();
    void*  p      = h.alloc_aligned(256, 64);
    ASSERT_NOT_NULL(p);
    h.free_aligned(p);
    ASSERT_GE(h.free_space(), before - 64);  // small slack
}

TEST(Heap_Aligned, FreeAlignedHeapValid, "validate_heap passes after alloc_aligned + free_aligned") {
    WITH_HEAP(h, 64);
    void* p = h.alloc_aligned(128, 32);
    ASSERT_NOT_NULL(p);
    h.free_aligned(p);
    ASSERT_TRUE(h.validate());
}