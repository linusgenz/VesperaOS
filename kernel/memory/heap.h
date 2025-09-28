//
// Created by linus on 13.10.24.
//

#ifndef HEAP_H
#define HEAP_H
#include <cstdint>
#include <cstddef>

#define HEAP_MAGIC_FREE     0xDEADBEEF
#define HEAP_MAGIC_USED     0xBEEFCAFE
#define HEAP_MAGIC_ALIGNED  0xCAFEBABE
#define HEAP_GUARD_PATTERN  0xAA

// Heap segment header size (cache-aligned)
#define HEAP_HEADER_SIZE    64
#define MIN_ALLOC_SIZE      16

struct HeapSegHdr {
    uint32_t magic;         // Corruption detection
    size_t length;          // Length of usable data (excluding header)
    HeapSegHdr* next;
    HeapSegHdr* last;
    bool free;
    uint8_t guard_start;    // Guard byte at start
    uint8_t reserved[2];    // Reserved for future use
    uint32_t checksum;      // Additional corruption detection
    uint64_t reserved2[1];  // Reserved space (total size = 64 bytes)

    // Methods
    HeapSegHdr* split(size_t split_length);
    void combine_forward();
    void combine_backward() const;
    bool is_valid() const;
    void set_guard_bytes();
    bool check_guard_bytes() const;

    // Get pointer to user data
    void* get_data_ptr() { return (void*)((uintptr_t)this + 64); }  // Now 64 bytes

    // Get header from user data pointer
    static HeapSegHdr* from_data_ptr(void* ptr) {
        return (HeapSegHdr*)((uintptr_t)ptr - 64);  // Now 64 bytes
    }
};

// Minimum alignment (16 bytes)
#define MIN_ALIGNMENT       16
struct AlignedSegHdr {
    uint32_t magic;         // HEAP_MAGIC_ALIGNED
    HeapSegHdr* raw_segment; // Pointer to the actual heap segment
    size_t user_size;       // Original requested size
    size_t alignment;       // Alignment used
    uint8_t guard_pattern[4]; // Guard bytes
    uint64_t reserved[1];

    bool is_valid() const {
        return magic == HEAP_MAGIC_ALIGNED &&
               guard_pattern[0] == HEAP_GUARD_PATTERN &&
               guard_pattern[1] == HEAP_GUARD_PATTERN &&
               guard_pattern[2] == HEAP_GUARD_PATTERN &&
               guard_pattern[3] == HEAP_GUARD_PATTERN;
    }

    void set_guard_bytes() {
        for (int i = 0; i < 4; i++) {
            guard_pattern[i] = HEAP_GUARD_PATTERN;
        }
    }
};

// Global heap variables
extern void* heap_start;
extern void* heap_end;
extern HeapSegHdr* last_hdr;
extern bool heap_initialized;

// Statistics
extern size_t total_allocated;
extern size_t total_freed;
extern size_t peak_usage;

// Core heap functions
bool initialize_heap(void* heap_address, size_t page_count);
void* malloc(size_t size);
void* alloc_aligned(size_t size, size_t alignment, size_t boundary = 0);
void* realloc(void* ptr, size_t old_size, size_t new_size);
void free(void* ptr);
void free_aligned(void* ptr);

// Utility functions
void expand_heap(size_t length);
size_t align_size(size_t size);
HeapSegHdr* find_free_segment(size_t size);
void* allocate_from_segment(HeapSegHdr* seg, size_t size);

// Debugging and validation functions
bool validate_heap();
void print_heap_stats();
bool is_valid_pointer(void* ptr);
size_t get_heap_usage();
size_t get_free_space();

inline void* operator new(size_t size) {return malloc(size);}
inline void* operator new[](size_t size) {return malloc(size);}
inline void* operator new(size_t size, void* ptr) noexcept {return ptr;}
inline void operator delete(void* p) {return free(p);}
inline void operator delete(void* ptr, size_t size) {free(ptr);}
inline void operator delete[](void *p) { free(p); }
inline void operator delete[](void *p, size_t size) { free(p); }



#endif //HEAP_H