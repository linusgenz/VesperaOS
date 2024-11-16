#include "heap.h"
#include "../include/page_table_manager.h"
#include "../include/page_frame_allocator.h"

void* heap_start;
void* heap_end;
HeapSegHdr* last_hdr;

void initialize_heap(void* heap_address, size_t page_count) {
    void* pos = heap_address;

    for (size_t i = 0; i < page_count; i++) {
        global_page_table_manager.map_memory(pos, global_allocator.request_page());
        pos = (void*)((size_t)pos + 0x1000);
    }

    size_t heap_length = page_count * 0x1000;

    heap_start = heap_address;
    heap_end = (void*)((size_t)heap_start + heap_length);
    HeapSegHdr* start_seg = (HeapSegHdr*)heap_address;
    start_seg->length = heap_length - sizeof(HeapSegHdr);
    start_seg->next = NULL;
    start_seg->last = NULL;
    start_seg->free = true;
    last_hdr = start_seg;
}

void free(void* addr) {
    HeapSegHdr* segment = (HeapSegHdr*)addr - 1;
    segment->free = true;
    segment->combine_forward();
    segment->combine_backward();
}


void* malloc(size_t size) {
    if (size % 0x10 > 0) { // not multiple of 0x10
        size -= (size % 0x10);
        size += 0x10;
    }

    if (size == 0) return NULL;

    HeapSegHdr* current_seg = (HeapSegHdr*) heap_start;
    while (true)
    {
        if (current_seg->free) {
            if (current_seg->length > size) {
                current_seg->split(size);
                current_seg->free = false;
                return (void*)((uint64_t)current_seg + sizeof(HeapSegHdr));
            }
            if (current_seg->length == size) {
                current_seg->free = false;
                return (void*)((uint64_t)current_seg + sizeof(HeapSegHdr));
            }
        }
        if (current_seg->next == NULL) break;
        current_seg = current_seg->next;
    }
    expand_heap(size);
    return malloc(size);
}


HeapSegHdr* HeapSegHdr::split(size_t split_length) {
    if (split_length < 0x10) return NULL;
    size_t split_seg_length = length - split_length - (sizeof(HeapSegHdr));
    if (split_seg_length < 0x10) return NULL;

    HeapSegHdr* new_split_hdr = (HeapSegHdr*) ((size_t)this + split_length + sizeof(HeapSegHdr));
    next->last = new_split_hdr;     // Ensure the next segment's last pointer is updated to point to the new split header.
    new_split_hdr->next = next;    // Set the new split header's next pointer to the original next segment.
    next = new_split_hdr;    // Update the current segment's next pointer to point to the newly created split segment.
    new_split_hdr->last = this;    // Set the last pointer of the new split header to point back to the current segment.
    new_split_hdr->length = split_seg_length;    // Set the length of the new split segment to the calculated split segment length.
    new_split_hdr->free = free;    // Set the free status of the new split segment to match the current segment's free status.
    length = split_length;    // Update the current segment's length to the requested split length.

    if (last_hdr == this) last_hdr = new_split_hdr;
    return new_split_hdr;
}

void expand_heap(size_t length) {
    if (length % 0x1000) {
        length -= length % 0x1000;
        length += 0x1000;
    }

    size_t page_count = length / 0x1000;
    HeapSegHdr* new_segment = (HeapSegHdr*)heap_end;
    for (size_t i = 0; i < page_count; i++) {
        global_page_table_manager.map_memory(heap_end, global_allocator.request_page());
        heap_end = (void*)((size_t)heap_end + 0x1000);
    }

    new_segment->free = true;
    new_segment->last = last_hdr;
    last_hdr->next = new_segment;
    last_hdr = new_segment;
    new_segment->next = NULL;
    new_segment->length = length - sizeof(HeapSegHdr);
    new_segment->combine_backward();
}

void HeapSegHdr::combine_forward() {
    if (next == NULL) return;
    if (!next->free) return;

    if (next == last_hdr) last_hdr = this;

    if (next->next != NULL) {
        next->next->last = this;
    }

    length = length + next->length + sizeof(HeapSegHdr);

    next = next->next;
}

void HeapSegHdr::combine_backward() {
    if (last != NULL && last->free) last->combine_forward();
}