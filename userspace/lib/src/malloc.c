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

#include <memory.h>
#include <string.h>
#include <sys/mman.h>
#include <sysstd.h>

#include "stdio.h"

#define HEAP_MAGIC 0xDEADBEEFu
#define MIN_ALLOC_SIZE 16
#define HEAP_ALIGN 16
#define LARGE_ALLOC_THRESHOLD (64 * 1024)

static heap_seg *g_heap_head = NULL;
static uintptr_t g_heap_base = 0;
static uintptr_t g_heap_end = 0;
static large_seg *g_large_alloc_list = NULL;

static inline size_t align_up(size_t n) {
    return (n + (HEAP_ALIGN - 1)) & ~(size_t)(HEAP_ALIGN - 1);
}

static heap_seg *find_free_segment(size_t size) {
    heap_seg *seg = g_heap_head;
    while (seg) {
        /* Guard against a partially-corrupted walk */
        if (seg->magic != HEAP_MAGIC) break;
        if (seg->free && seg->length >= size) return seg;
        seg = seg->next;
    }
    return NULL;
}

static void split_segment(heap_seg *seg, size_t size) {
    if (!seg) return;
    if (seg->length <= size + sizeof(heap_seg) + MIN_ALLOC_SIZE) return;

    heap_seg *new_seg = (heap_seg *)((char *)seg + sizeof(heap_seg) + size);
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
    heap_seg *next = seg->next;
    if (next && next->free && next->magic == HEAP_MAGIC) {
        seg->length += sizeof(heap_seg) + next->length;
        seg->next = next->next;
        if (seg->next) seg->next->prev = seg;
    }

    heap_seg *prev = seg->prev;
    if (prev && prev->free && prev->magic == HEAP_MAGIC) {
        prev->length += sizeof(heap_seg) + seg->length;
        prev->next = seg->next;
        if (prev->next) prev->next->prev = prev;
    }
}

void heap_lazy_init(void) {
    if (g_heap_head) return;

    g_heap_base = (uintptr_t)sys_brk(0, 0, 0, 0, 0, 0);
    uintptr_t new_brk = g_heap_base + 0x20000;

    if ((uintptr_t)sys_brk(new_brk, 0, 0, 0, 0, 0) != new_brk) return;

    g_heap_end = new_brk;
    g_heap_head = (heap_seg *)g_heap_base;

    g_heap_head->length = 0x20000 - sizeof(heap_seg);
    g_heap_head->free = 1;
    g_heap_head->magic = HEAP_MAGIC;
    g_heap_head->next = NULL;
    g_heap_head->prev = NULL;
}

void *malloc(size_t size) {
    if (size == 0) return NULL;

    if (size >= LARGE_ALLOC_THRESHOLD) {
        size_t total = size + sizeof(large_seg);
        void *addr = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, 0, 0);
        if (addr == MAP_FAILED) return NULL;

        large_seg *lb = (large_seg *)addr;
        lb->addr = (char *)addr + sizeof(large_seg);
        lb->size = size;
        lb->next = g_large_alloc_list;
        g_large_alloc_list = lb;
        return lb->addr;
    }

    heap_lazy_init();
    if (!g_heap_head) return NULL;

    size = align_up(size);
    if (size < MIN_ALLOC_SIZE) size = MIN_ALLOC_SIZE;

    heap_seg *seg = find_free_segment(size);

    if (!seg) {
        size_t grow_size = align_up(size > 4096 ? size : 4096);

        /* Walk to the last segment */
        heap_seg *last = g_heap_head;
        while (last->next) last = last->next;

        if (last->free && last->magic == HEAP_MAGIC) {
            uintptr_t new_brk = g_heap_end + grow_size;
            if ((uintptr_t)sys_brk(new_brk, 0, 0, 0, 0, 0) != new_brk) return NULL;
            last->length += grow_size;
            g_heap_end = new_brk;
            seg = last;
        } else {
            uintptr_t new_brk = g_heap_end + sizeof(heap_seg) + grow_size;
            if ((uintptr_t)sys_brk(new_brk, 0, 0, 0, 0, 0) != new_brk) return NULL;

            seg = (heap_seg *)g_heap_end;
            seg->length = grow_size;
            seg->free = 1;
            seg->magic = HEAP_MAGIC;
            seg->next = NULL;
            seg->prev = last;
            last->next = seg;
            g_heap_end = new_brk;
        }
    }

    split_segment(seg, size);
    seg->free = 0;
    return (char *)seg + sizeof(heap_seg);
}

void heap_free(void *ptr) {
    if (!ptr) return;
    heap_seg *seg = (heap_seg *)((char *)ptr - sizeof(heap_seg));

    if (seg->magic != HEAP_MAGIC) return;
    if (seg->free) return;

    seg->free = 1;
    combine_segments(seg);
}

void free(void *ptr) {
    if (!ptr) return;

    /* Check large-alloc list first */
    large_seg **cur = &g_large_alloc_list;
    while (*cur) {
        if ((*cur)->addr == ptr) {
            size_t total = (*cur)->size + sizeof(large_seg);
            void *base = (char *)ptr - sizeof(large_seg);
            large_seg *nx = (*cur)->next;
            munmap(base, total);
            *cur = nx;
            return;
        }
        cur = &(*cur)->next;
    }

    heap_free(ptr);
}

void *realloc(void *ptr, size_t new_size) {
    if (!ptr) return malloc(new_size);
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    for (large_seg *lb = g_large_alloc_list; lb; lb = lb->next) {
        if (lb->addr == ptr) {
            if (lb->size >= new_size) return ptr;

            void *new_ptr = malloc(new_size);
            if (!new_ptr) return NULL;
            memcpy(new_ptr, ptr, lb->size);
            free(ptr);
            return new_ptr;
        }
    }

    heap_seg *seg = (heap_seg *)((char *)ptr - sizeof(heap_seg));
    if (seg->magic != HEAP_MAGIC) return NULL;

    size_t aligned_new = align_up(new_size);
    if (aligned_new < MIN_ALLOC_SIZE) aligned_new = MIN_ALLOC_SIZE;

    if (seg->length >= aligned_new) {
        split_segment(seg, aligned_new);
        return ptr;
    }

    void *new_ptr = malloc(new_size);
    if (!new_ptr) return NULL;
    memcpy(new_ptr, ptr, seg->length < new_size ? seg->length : new_size);
    free(ptr);
    return new_ptr;
}

void *calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return NULL;

    if (size > SIZE_MAX / nmemb) return NULL;

    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (!ptr) return NULL;

    if (total < LARGE_ALLOC_THRESHOLD) memset(ptr, 0, total);

    return ptr;
}