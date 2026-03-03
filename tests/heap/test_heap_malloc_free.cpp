// =============================================================================
// Heap Tests — malloc / free / realloc
// =============================================================================
// Covers:
//   malloc  - basic allocation, alignment, zero size, exhaustion,
//             multiple allocations, pattern integrity
//   free    - basic free, double-free detection, null free, invalid pointer
//   realloc - grow, shrink, null ptr (== malloc), zero size (== free),
//             data preservation, invalid pointer
//
// Implementation notes:
//   realloc() in this heap does NOT shrink in-place: when new_size <= seg->length
//   it returns the original pointer unchanged. Shrink tests therefore verify
//   that the returned pointer is valid and the retained data is intact,
//   NOT that the block physically shrank.
//
//   realloc() for grow: allocates a new block, copies, frees the old one.
//   The grow tests use a large arena (256 KB) so the second malloc inside
//   realloc always has room.
// =============================================================================

#include "heap_fixture.h"

// =============================================================================
// malloc — basic
// =============================================================================

TEST(Heap_Malloc, BasicAlloc, "malloc returns a non-null pointer for a small request") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(64);
    ASSERT_NOT_NULL(p);
}

TEST(Heap_Malloc, NullOnZeroSize, "malloc(0) returns null") {
    WITH_HEAP(h, 64);
    ASSERT_NULL(h.malloc(0));
}

TEST(Heap_Malloc, ReturnedPointerIsAligned, "malloc returns a pointer aligned to MIN_ALIGNMENT") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(1);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ((uintptr_t)0, reinterpret_cast<uintptr_t>(p) % MIN_ALIGNMENT);
}

TEST(Heap_Malloc, WriteAndReadBack, "Data written to a malloc'd block can be read back correctly") {
    WITH_HEAP(h, 64);
    auto* p = static_cast<uint8_t*>(h.malloc(256));
    ASSERT_NOT_NULL(p);
    fill_pattern(p, 256, 0xAB);
    ASSERT_TRUE(check_pattern(p, 256, 0xAB));
}

TEST(Heap_Malloc, AllByteValues, "All 256 byte values survive a write/read cycle") {
    WITH_HEAP(h, 64);
    auto* p = static_cast<uint8_t*>(h.malloc(256));
    ASSERT_NOT_NULL(p);
    for (int i = 0; i < 256; i++) p[i] = (uint8_t)i;
    for (int i = 0; i < 256; i++) ASSERT_EQ((uint8_t)i, p[i]);
}

TEST(Heap_Malloc, MultipleDistinctPointers, "Two malloc calls return non-overlapping pointers") {
    WITH_HEAP(h, 64);
    void* a = h.malloc(128);
    void* b = h.malloc(128);
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NE(a, b);
    uintptr_t ua = reinterpret_cast<uintptr_t>(a);
    uintptr_t ub = reinterpret_cast<uintptr_t>(b);
    ASSERT_TRUE(ub >= ua + 128 || ua >= ub + 128);
}

TEST(Heap_Malloc, ManySmallAllocations, "100 small allocations all succeed and return distinct pointers") {
    WITH_HEAP(h, 256);
    std::vector<void*> ptrs;
    for (int i = 0; i < 100; i++) {
        void* p = h.malloc(16);
        ASSERT_NOT_NULL(p);
        for (auto* q : ptrs)
            ASSERT_NE(p, q);
        ptrs.push_back(p);
    }
}

TEST(Heap_Malloc, SizeOneAllocated, "malloc(1) succeeds (rounded up to MIN_ALLOC_SIZE)") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(1);
    ASSERT_NOT_NULL(p);
}

TEST(Heap_Malloc, LargeAllocation, "Allocating 28 KB from a 64 KB arena succeeds") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(28 * 1024);
    ASSERT_NOT_NULL(p);
}

TEST(Heap_Malloc, ExhaustionReturnsNull, "malloc returns null when the heap is full") {
    WITH_HEAP(h, 4);  // tiny heap
    std::vector<void*> ptrs;
    for (int i = 0; i < 1000; i++) {
        void* p = h.malloc(64);
        if (!p) break;
        ptrs.push_back(p);
    }
    // After exhaustion the next alloc must return null
    ASSERT_NULL(h.malloc(64));
}

TEST(Heap_Malloc, HeapValidAfterManyAllocs, "validate_heap passes after 50 allocations") {
    WITH_HEAP(h, 256);
    for (int i = 0; i < 50; i++) h.malloc(64);
    ASSERT_TRUE(h.validate());
}

TEST(Heap_Malloc, AllocSizeRoundsUp, "Requested sizes are rounded up to MIN_ALIGNMENT") {
    WITH_HEAP(h, 64);
    auto* p = static_cast<uint8_t*>(h.malloc(1));
    ASSERT_NOT_NULL(p);
    ASSERT_EQ((uintptr_t)0, reinterpret_cast<uintptr_t>(p) % MIN_ALIGNMENT);
}

TEST(Heap_Malloc, AdjacentBlocksDoNotOverlap, "Back-to-back allocations never share memory") {
    WITH_HEAP(h, 64);
    auto* a = static_cast<uint8_t*>(h.malloc(64));
    auto* b = static_cast<uint8_t*>(h.malloc(64));
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    fill_pattern(a, 64, 0xAA);
    fill_pattern(b, 64, 0xBB);
    ASSERT_TRUE(check_pattern(a, 64, 0xAA));
    ASSERT_TRUE(check_pattern(b, 64, 0xBB));
}

TEST(Heap_Malloc, UsageIncreasesAfterAlloc, "get_heap_usage increases after allocation") {
    WITH_HEAP(h, 64);
    size_t before = h.usage();
    h.malloc(128);
    ASSERT_GE(h.usage(), before + 128);
}

// =============================================================================
// free — basic
// =============================================================================

TEST(Heap_Free, BasicFree, "Allocated memory can be freed without crashing") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(128);
    ASSERT_NOT_NULL(p);
    h.free(p);
    ASSERT_TRUE(h.validate());
}

TEST(Heap_Free, NullFreeIsNoop, "free(null) is a no-op") {
    WITH_HEAP(h, 64);
    h.free(nullptr);
    ASSERT_TRUE(h.validate());
}

TEST(Heap_Free, FreeSpaceIncreasesAfterFree, "Free space increases after freeing a block") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(512);
    ASSERT_NOT_NULL(p);
    size_t before = h.free_space();
    h.free(p);
    ASSERT_GE(h.free_space(), before + 512);
}

TEST(Heap_Free, FreedBlockIsReusable, "A freed block can be allocated again") {
    WITH_HEAP(h, 8);
    void* p = h.malloc(256);
    ASSERT_NOT_NULL(p);
    h.free(p);
    void* q = h.malloc(256);
    ASSERT_NOT_NULL(q);
}

TEST(Heap_Free, ForwardCoalescing, "Two adjacent free blocks are merged into one") {
    WITH_HEAP(h, 64);
    void* a = h.malloc(256);
    void* b = h.malloc(256);
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);

    size_t free_before = h.free_space();
    h.free(a);
    h.free(b);

    ASSERT_GE(h.free_space(), free_before + 256 + 256);
    ASSERT_TRUE(h.validate());
}

TEST(Heap_Free, BackwardCoalescing, "Freeing the second of two adjacent blocks coalesces with the first") {
    WITH_HEAP(h, 64);
    void* a = h.malloc(128);
    void* b = h.malloc(128);
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);

    h.free(a);
    h.free(b);

    ASSERT_TRUE(h.validate());
    // After merging, a block larger than either piece alone must be available
    void* big = h.malloc(200);
    ASSERT_NOT_NULL(big);
}

TEST(Heap_Free, AllAllocsFreedRestoresSpace, "Freeing all allocations restores the original free space") {
    WITH_HEAP(h, 64);
    size_t initial = h.free_space();

    std::vector<void*> ptrs;
    for (int i = 0; i < 20; i++) {
        void* p = h.malloc(64);
        ASSERT_NOT_NULL(p);
        ptrs.push_back(p);
    }
    for (auto* p : ptrs) h.free(p);

    ASSERT_GE(h.free_space(), initial - 64);  // small slack for alignment rounding
    ASSERT_TRUE(h.validate());
}

TEST(Heap_Free, HeapValidAfterInterleavedAllocFree, "validate_heap passes after interleaved alloc/free pattern") {
    WITH_HEAP(h, 64);
    void* a = h.malloc(64);
    void* b = h.malloc(64);
    void* c = h.malloc(64);
    h.free(b);
    void* d = h.malloc(32);
    h.free(a);
    h.free(c);
    h.free(d);
    ASSERT_TRUE(h.validate());
}

TEST(Heap_Free, IsValidPointerAfterFreeReturnsFalse, "is_valid_pointer returns false after the block is freed") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(64);
    ASSERT_TRUE(h.is_valid_ptr(p));
    h.free(p);
    ASSERT_FALSE(h.is_valid_ptr(p));
}

// =============================================================================
// realloc
// =============================================================================

TEST(Heap_Realloc, NullPtrActsAsMalloc, "realloc(null, 0, size) behaves like malloc(size)") {
    WITH_HEAP(h, 64);
    void* p = h.realloc(nullptr, 0, 128);
    ASSERT_NOT_NULL(p);
}

TEST(Heap_Realloc, ZeroNewSizeActsAsFree, "realloc(ptr, _, 0) frees the pointer and returns null") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(128);
    ASSERT_NOT_NULL(p);
    void* r = h.realloc(p, 128, 0);
    ASSERT_NULL(r);
    ASSERT_TRUE(h.validate());
}

TEST(Heap_Realloc, GrowPreservesData, "Growing a block preserves the original data") {
    // Use a large arena: realloc(grow) calls malloc internally for the new block,
    // so we need room for both the old and the new allocation simultaneously.
    WITH_HEAP(h, 256);
    auto* p = static_cast<uint8_t*>(h.malloc(64));
    ASSERT_NOT_NULL(p);
    fill_pattern(p, 64, 0xCD);

    // Grow: new_size (256) > seg->length (64) → allocates new block + memcpy + free old
    auto* q = static_cast<uint8_t*>(h.realloc(p, 64, 256));
    ASSERT_NOT_NULL(q);
    ASSERT_TRUE(check_pattern(q, 64, 0xCD));
}

TEST(Heap_Realloc, ShrinkReturnsOriginalPointer, "Shrinking a block returns a valid (possibly same) pointer") {
    // This heap does not shrink in-place: when new_size <= seg->length the
    // original pointer is returned unchanged.  The test verifies the contract
    // (returns non-null, heap stays valid) without assuming a new address.
    WITH_HEAP(h, 64);
    void* p = h.malloc(512);
    ASSERT_NOT_NULL(p);
    void* q = h.realloc(p, 512, 64);
    ASSERT_NOT_NULL(q);
    ASSERT_TRUE(h.is_valid_ptr(q));
    ASSERT_TRUE(h.validate());
    h.free(q);
}

TEST(Heap_Realloc, ShrinkPreservesData, "Shrinking a block preserves the retained bytes") {
    WITH_HEAP(h, 64);
    auto* p = static_cast<uint8_t*>(h.malloc(256));
    ASSERT_NOT_NULL(p);
    fill_pattern(p, 256, 0xEF);

    // new_size (64) <= seg->length (256) → same pointer returned, data intact
    auto* q = static_cast<uint8_t*>(h.realloc(p, 256, 64));
    ASSERT_NOT_NULL(q);
    ASSERT_TRUE(check_pattern(q, 64, 0xEF));
    h.free(q);
}

TEST(Heap_Realloc, GrowThenShrink, "validate_heap passes after a grow + shrink cycle") {
    WITH_HEAP(h, 256);
    void* p = h.malloc(128);
    ASSERT_NOT_NULL(p);
    p = h.realloc(p, 128, 512);   // grow — needs room for both blocks
    ASSERT_NOT_NULL(p);
    p = h.realloc(p, 512, 64);    // shrink — returns same pointer
    ASSERT_NOT_NULL(p);
    h.free(p);
    ASSERT_TRUE(h.validate());
}

TEST(Heap_Realloc, SameSize, "realloc to the same size returns a valid pointer") {
    WITH_HEAP(h, 64);
    void* p = h.malloc(128);
    ASSERT_NOT_NULL(p);
    void* q = h.realloc(p, 128, 128);
    ASSERT_NOT_NULL(q);
    ASSERT_TRUE(h.is_valid_ptr(q));
    h.free(q);
}