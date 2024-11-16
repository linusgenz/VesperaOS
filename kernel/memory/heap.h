//
// Created by linus on 13.10.24.
//

#ifndef HEAP_H
#define HEAP_H
#include <stdint.h>
#include <stddef.h>

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

void* malloc(size_t size);
void free(void* addr);

void expand_heap(size_t length);

inline void* operator new(size_t size) {return malloc(size);}
inline void* operator new[](size_t size) {return malloc(size);}
inline void operator delete(void* p) {return free(p);}



#endif //HEAP_H