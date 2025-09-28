// malloc.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 27.09.25.
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

#include <sysstd.h>
#include <memory.h>
#include <string.h>
#include <sys/mman.h>

#define HEAP_MAGIC 0xDEADBEEF
#define MIN_ALLOC_SIZE 16

#define LARGE_ALLOC_THRESHOLD (64*1024)

static heap_seg *heap_head = nullptr;
static uintptr_t heap_base = 0;
static uintptr_t heap_end = 0;

static large_seg* large_alloc_list = nullptr;

static heap_seg *find_free_segment(size_t size) {
    heap_seg *seg = heap_head;
    while (seg) {
        if (seg->free && seg->length >= size) return seg;
        seg = seg->next;
    }
    return nullptr;
}

static void split_segment(heap_seg *seg, size_t size) {
    if (!seg || seg->length <= size + sizeof(heap_seg) + MIN_ALLOC_SIZE) return;

    heap_seg *new_seg = (heap_seg *) ((char *) seg + sizeof(heap_seg) + size);
    new_seg->length = seg->length - size - sizeof(heap_seg);
    new_seg->free = 1;
    new_seg->magic = HEAP_MAGIC;
    new_seg->next = seg->next;
    new_seg->prev = seg;
    if (seg->next) seg->next->prev = new_seg;
    seg->next = new_seg;
    seg->length = size;
}


static void combine_segments(heap_seg *seg) {
    if (seg->next && seg->next->free && seg->next->magic == HEAP_MAGIC) {
        seg->length += sizeof(heap_seg) + seg->next->length;
        seg->next = seg->next->next;
        if (seg->next) seg->next->prev = seg;
    }
    if (seg->prev && seg->prev->free && seg->prev->magic == HEAP_MAGIC) {
        seg = seg->prev;
        seg->length += sizeof(heap_seg) + seg->next->length;
        seg->next = seg->next->next;
        if (seg->next) seg->next->prev = seg;
    }
}

void heap_lazy_init() {
    if (heap_head) return;

    heap_base = sys_brk(0, 0, 0, 0, 0, 0);
    heap_end = heap_base;

    // initialize first segment
    heap_head = (heap_seg *) heap_end;
    heap_head->length = 0;
    heap_head->free = 1;
    heap_head->magic = HEAP_MAGIC;
    heap_head->next = nullptr;
    heap_head->prev = nullptr;
}

void *malloc(size_t size) {
    if (size == 0) return nullptr;

    if (size >= LARGE_ALLOC_THRESHOLD) {
        void* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, 0, 0);
        if (!addr) return nullptr;

        large_seg* lb = (large_seg*)malloc(sizeof(large_seg));
        if (!lb) return nullptr;
        lb->addr = addr;
        lb->size = size;
        lb->next = large_alloc_list;
        large_alloc_list = lb;

        return addr;
    }
    
    heap_lazy_init();
    
    size = (size + 7) & ~7; // align to 8 bytes
    heap_seg *seg = find_free_segment(size);

    if (!seg) {
        // grow heap
        uintptr_t new_brk = heap_end + sizeof(heap_seg) + size;
        if (sys_brk(new_brk, 0, 0, 0, 0, 0) != new_brk) return nullptr; // failed
        seg = (heap_seg *) heap_end;
        seg->length = size;
        seg->free = 0;
        seg->magic = HEAP_MAGIC;
        seg->next = nullptr;
        seg->prev = nullptr;

        if (heap_head) {
            heap_seg *last = heap_head;
            while (last->next) last = last->next;
            last->next = seg;
            seg->prev = last;
        } else {
            heap_head = seg;
        }

        heap_end = new_brk;
        return (char *) seg + sizeof(heap_seg);
    }

    split_segment(seg, size);
    seg->free = 0;
    return (char *) seg + sizeof(heap_seg);
}

void heap_free(void *ptr) {
    if (!ptr) return;
    heap_seg *seg = (heap_seg *) ((char *) ptr - sizeof(heap_seg));
    seg->free = 1;
    combine_segments(seg);
}

void free(void* ptr) {
    if (!ptr) return;

    // Prüfen, ob es sich um eine große mmap-Allokation handelt
    large_seg** current = &large_alloc_list;
    while (*current) {
        if ((*current)->addr == ptr) {
            munmap(ptr, (*current)->size);
            large_seg* to_free = *current;
            *current = (*current)->next;
            heap_free(to_free); // Tracking-Struktur freigeben
            return;
        }
        current = &(*current)->next;
    }

    // Normale kleine Allokation
    heap_free(ptr);
}



void *realloc(void *ptr, size_t new_size) {
    if (!ptr) return malloc(new_size);
    if (new_size == 0) {
        free(ptr);
        return nullptr;
    }

    heap_seg *seg = (heap_seg *) ((char *) ptr - sizeof(heap_seg));
    if (seg->length >= new_size) return ptr;

    void *new_ptr = malloc(new_size);
    if (!new_ptr) return nullptr;

    memcpy(new_ptr, ptr, seg->length);
    free(ptr);
    return new_ptr;
}
