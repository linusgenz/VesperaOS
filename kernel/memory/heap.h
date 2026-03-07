//
// Created by linus on 13.10.24.
//

#ifndef HEAP_H
#define HEAP_H

#include <vespera/types.h>

#include <vespera/mm/addr.h>

#define HEAP_MAGIC_FREE 0xDEADBEEF
#define HEAP_MAGIC_USED 0xBEEFCAFE
#define HEAP_MAGIC_ALIGNED 0xCAFEBABE
#define HEAP_GUARD_PATTERN 0xAA

// Heap segment header size (cache-aligned)
#define HEAP_HEADER_SIZE 64
#define MIN_ALLOC_SIZE 16

struct HeapSegHdr {
    u32 magic;  // Corruption detection
    usize length;   // Length of usable data (excluding header)
    HeapSegHdr *next;
    HeapSegHdr *last;
    bool free;
    u8 guard_start;    // Guard byte at start
    u8 reserved[2];    // Reserved for future use
    u32 checksum;      // Additional corruption detection
    u64 reserved2[3];  // Reserved space (total size = 64 bytes)

    // Methods
    HeapSegHdr *split(usize split_length);

    void combine_forward();

    void combine_backward() const;

    bool is_valid() const;

    void set_guard_bytes();

    bool check_guard_bytes() const;

    void *get_data_ptr() {
        return reinterpret_cast<void *>(reinterpret_cast<uptr>(this) + HEAP_HEADER_SIZE);
    }

    static HeapSegHdr *from_data_ptr(void *ptr) {
        return reinterpret_cast<HeapSegHdr *>(reinterpret_cast<uptr>(ptr) - HEAP_HEADER_SIZE);
    }
};

// Minimum alignment (16 bytes)
constexpr usize MIN_ALIGNMENT = 16;

struct AlignedSegHdr {
    u32 magic;            // HEAP_MAGIC_ALIGNED
    HeapSegHdr *raw_segment;   // Pointer to the actual heap segment
    usize user_size;          // Original requested size
    usize alignment;          // Alignment used
    u8 guard_pattern[4];  // Guard bytes
    u64 reserved[3];

    bool is_valid() const {
        return magic == HEAP_MAGIC_ALIGNED && guard_pattern[0] == HEAP_GUARD_PATTERN &&
               guard_pattern[1] == HEAP_GUARD_PATTERN && guard_pattern[2] == HEAP_GUARD_PATTERN &&
               guard_pattern[3] == HEAP_GUARD_PATTERN;
    }

    void set_guard_bytes() {
        for (int i = 0; i < 4; i++) {
            guard_pattern[i] = HEAP_GUARD_PATTERN;
        }
    }
};

// Global heap variables
extern bool heap_initialized;

// Statistics
extern usize total_allocated;
extern usize total_freed;
extern usize peak_usage;

// Core heap functions
bool initialize_heap(virt_addr_t heap_address, usize page_count);

void *kmalloc(usize size);

void *kalloc_aligned(usize size, usize alignment, usize boundary = 0);

void *krealloc(void *ptr, usize old_size, usize new_size);

void kfree(void *ptr);

void kfree_aligned(void *ptr);

// Utility functions
void expand_heap(usize length);

usize align_size(usize size);

HeapSegHdr *find_free_segment(usize size);

void *allocate_from_segment(HeapSegHdr *seg, usize size);

// Debugging and validation functions
bool validate_heap();

void print_heap_stats();

bool is_valid_pointer(void *ptr);

usize get_heap_usage();

usize get_free_space();

#endif  // HEAP_H
