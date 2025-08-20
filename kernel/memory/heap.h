//
// Created by linus on 13.10.24.
//

#ifndef HEAP_H
#define HEAP_H
#include <cstdint>
#include <cstddef>

typedef struct HeapSegHdr {
    size_t length;
    HeapSegHdr* next;
    HeapSegHdr* last;
    bool free;
    void combine_forward();
    void combine_backward();
    HeapSegHdr* split(size_t split_length);
} HeapSegHdr;


void initialize_heap(void* heap_address, size_t page_count);

void* alloc_aligned(size_t alignment, size_t size, size_t boundary = 0);
void free_aligned(void* aligned_ptr);
void* malloc(size_t size);
void free(void* addr);
void* realloc(void* oldPtr, size_t oldSize, size_t newSize);

void expand_heap(size_t length);

inline void* operator new(size_t size) {return malloc(size);}
inline void* operator new[](size_t size) {return malloc(size);}
inline void* operator new(size_t size, void* ptr) noexcept {return ptr;}
inline void operator delete(void* p) {return free(p);}
inline void operator delete(void* ptr, size_t size) {free(ptr);}



#endif //HEAP_H