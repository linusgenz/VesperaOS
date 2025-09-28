#include "heap.h"
#include "../include/memory.h"
#include <log.h>
void* heap_start = nullptr;
void* heap_end = nullptr;
HeapSegHdr* last_hdr = nullptr;
bool heap_initialized = false;

// Statistics
size_t total_allocated = 0;
size_t total_freed = 0;
size_t peak_usage = 0;

bool HeapSegHdr::is_valid() const {
    return (magic == HEAP_MAGIC_FREE || magic == HEAP_MAGIC_USED) &&
           length > 0 && length < 0x100000000ULL; // Reasonable size limit
}

void HeapSegHdr::set_guard_bytes() {
    guard_start = HEAP_GUARD_PATTERN;
    // Set guard byte at end of user data
    uint8_t* end_guard = (uint8_t*)((uintptr_t)this + HEAP_HEADER_SIZE + length);
    if ((uintptr_t)end_guard < (uintptr_t)heap_end) {
        *end_guard = HEAP_GUARD_PATTERN;
    }

    // Calculate simple checksum for corruption detection
    checksum = magic ^ (uint32_t)length ^ (uint32_t)((uintptr_t)next >> 32) ^ (uint32_t)(uintptr_t)next;
}

bool HeapSegHdr::check_guard_bytes() const {
    if (guard_start != HEAP_GUARD_PATTERN) return false;

    // Verify checksum
    uint32_t expected_checksum = magic ^ (uint32_t)length ^ (uint32_t)((uintptr_t)next >> 32) ^ (uint32_t)(uintptr_t)next;
    if (checksum != expected_checksum) return false;

    uint8_t* end_guard = (uint8_t*)((uintptr_t)this + HEAP_HEADER_SIZE + length);
    if ((uintptr_t)end_guard < (uintptr_t)heap_end) {
        return *end_guard == HEAP_GUARD_PATTERN;
    }
    return true;
}

HeapSegHdr* HeapSegHdr::split(size_t split_length) {
    if (!is_valid()) {
        Log::Error("Split failed: invalid segment header at %p", this);
        return nullptr;
    }
    if (!free) {
        Log::Error("Split failed: can only split free segments");
        return nullptr;
    }
    if (split_length < MIN_ALLOC_SIZE) {
        Log::Error("Split failed: split length %zu too small (min: %u)", split_length, MIN_ALLOC_SIZE);
        return nullptr;
    }

    size_t total_here = HEAP_HEADER_SIZE + length + 1;
    size_t needed_for_split = HEAP_HEADER_SIZE + split_length + 1 + MIN_ALLOC_SIZE + HEAP_HEADER_SIZE; // conservative
    if (length < split_length + HEAP_HEADER_SIZE + MIN_ALLOC_SIZE + 1) {
        Log::debug("Split not possible: remaining would be too small (seg len=%zu, req=%zu)", length, split_length);
        return nullptr;
    }

    // compute address of new segment header
    uintptr_t new_seg_addr = (uintptr_t)this + HEAP_HEADER_SIZE + split_length + 1;
    HeapSegHdr* new_seg = reinterpret_cast<HeapSegHdr*>(new_seg_addr);

    // remaining length for new seg (user-data)
    size_t remaining_length = this->length - split_length - HEAP_HEADER_SIZE - 1;
    if (remaining_length < MIN_ALLOC_SIZE) {
        Log::Error("Split failed: remaining length %zu too small after split", remaining_length);
        return nullptr;
    }

    new_seg->magic = HEAP_MAGIC_FREE;
    new_seg->length = remaining_length;
    new_seg->free = true;
    new_seg->last = this;
    new_seg->next = this->next;
    new_seg->set_guard_bytes();

    if (new_seg->next) {
        new_seg->next->last = new_seg;
    }

    this->length = split_length;
    this->next = new_seg;
    this->set_guard_bytes();

    if (last_hdr == this) {
        last_hdr = new_seg;
    }

    return new_seg;
}


void HeapSegHdr::combine_forward() {
    // combine this and next if both free and valid
    if (!next || !next->free || !is_valid() || !next->is_valid()) {
        return;
    }

    if (next == last_hdr) {
        last_hdr = this;
    }

    this->length += HEAP_HEADER_SIZE + 1 + next->length;

    HeapSegHdr* after = next->next;
    this->next = after;
    if (after) {
        after->last = this;
    }

    this->set_guard_bytes();
}

void HeapSegHdr::combine_backward() const {
    if (last && last->free && last->is_valid()) {
        last->combine_forward();
    }
}


// Core heap function implementations
bool initialize_heap(void* heap_address, size_t page_count) {
    if (heap_initialized) {
        Log::Error("Heap already initialized");
        return false;
    }
    if (!heap_address) {
        Log::Error("Invalid heap address (null pointer)");
        return false;
    }
    if (page_count == 0) {
        Log::Error("Invalid page count (zero pages)");
        return false;
    }

    void* pos = heap_address;

    // Map pages
    for (size_t i = 0; i < page_count; i++) {
        kernel::memory::map_memory(pos, kernel::memory::request_page());
        pos = (void*)((size_t)pos + 0x1000);
    }

    size_t heap_length = page_count * 0x1000;

    heap_start = heap_address;
    heap_end = (void*)((size_t)heap_start + heap_length);

    // Initialize first segment
    HeapSegHdr* start_seg = (HeapSegHdr*)heap_address;
    start_seg->magic = HEAP_MAGIC_FREE;
    start_seg->length = heap_length - HEAP_HEADER_SIZE - 1; // -1 for end guard
    start_seg->next = nullptr;
    start_seg->last = nullptr;
    start_seg->free = true;
    start_seg->set_guard_bytes();

    last_hdr = start_seg;
    heap_initialized = true;

    // Reset statistics
    total_allocated = 0;
    total_freed = 0;
    peak_usage = 0;

    return true;
}

size_t align_size(size_t size) {
    if (size == 0) return MIN_ALLOC_SIZE;

    if (size % MIN_ALIGNMENT != 0) {
        size = (size + MIN_ALIGNMENT - 1) & ~(MIN_ALIGNMENT - 1);
    }

    return size < MIN_ALLOC_SIZE ? MIN_ALLOC_SIZE : size;
}

HeapSegHdr* find_free_segment(size_t size) {
    if (!heap_initialized) return nullptr;

    HeapSegHdr* current_seg = (HeapSegHdr*)heap_start;

    while (current_seg) {
        if (!current_seg->is_valid()) {
            // Heap corruption detected
            Log::Error("Heap segment is not valid");
            return nullptr;
        }

        if (current_seg->free && current_seg->length >= size) {
            return current_seg;
        }

        current_seg = current_seg->next;
    }

    return nullptr;
}

void* allocate_from_segment(HeapSegHdr* seg, size_t size) {
    if (!seg || !seg->free || seg->length < size) {
        return nullptr;
    }

    if (seg->length >= size + HEAP_HEADER_SIZE + MIN_ALLOC_SIZE + 1) {
        HeapSegHdr* new_seg = seg->split(size);
        if (!new_seg) {
            Log::Warning("Split failed, taking whole segment");
        }
    }

    seg->free = false;
    seg->magic = HEAP_MAGIC_USED;
    seg->set_guard_bytes();

    total_allocated += seg->length;
    if (total_allocated - total_freed > peak_usage) {
        peak_usage = total_allocated - total_freed;
    }

    return seg->get_data_ptr();
}



void* malloc(size_t size) {
    if (!heap_initialized || size == 0) {
        return nullptr;
    }

    size = align_size(size);

    HeapSegHdr* seg = find_free_segment(size);
    if (seg) {
        return allocate_from_segment(seg, size);
    }

    // Need to expand heap
    expand_heap(size);
    seg = find_free_segment(size);
    if (seg) {
        return allocate_from_segment(seg, size);
    }

    return nullptr;
}

void* alloc_aligned(size_t size, size_t alignment, size_t boundary) {
    if (!heap_initialized || size == 0 || alignment == 0) {
        return nullptr;
    }

    // Validate alignment (must be power of 2)
    if ((alignment & (alignment - 1)) != 0) {
        Log::Error("Alignment must be a power of 2: %u", alignment);
        return nullptr;
    }

    // Validate boundary if specified
    if (boundary != 0 && (boundary & (boundary - 1)) != 0) {
        Log::Error("Bounds must be a power of 2");
        return nullptr;
    }

    if (alignment < MIN_ALIGNMENT) {
        alignment = MIN_ALIGNMENT;
    }

    size = align_size(size);

    // Calculate total size needed
    size_t header_size = sizeof(AlignedSegHdr);
    size_t max_padding = alignment - 1 + header_size;
    size_t total_size = size + max_padding;

    if (boundary > 0) {
        total_size += boundary; // Extra space for boundary alignment
    }

    // Allocate raw memory
    void* raw_ptr = malloc(total_size);
    if (!raw_ptr) {
        Log::Error("Aligned alloc failed: could not allocate %u bytes for aligned allocation", total_size);
        return nullptr;
    }

    HeapSegHdr* raw_seg = HeapSegHdr::from_data_ptr(raw_ptr);

    // Calculate aligned address
    uintptr_t raw_addr = (uintptr_t)raw_ptr;
    uintptr_t aligned_addr = (raw_addr + header_size + alignment - 1) & ~(alignment - 1);

    // Handle boundary constraint
    if (boundary > 0) {
        uintptr_t end_addr = raw_addr + total_size;

        // Find suitable aligned address within boundary
        bool found = false;
        for (uintptr_t candidate = aligned_addr;
             candidate + size <= end_addr;
             candidate += alignment) {

            uintptr_t start_boundary = candidate & ~(boundary - 1);
            uintptr_t end_boundary = (candidate + size - 1) & ~(boundary - 1);

            if (start_boundary == end_boundary) {
                aligned_addr = candidate;
                found = true;
                break;
            }
        }

        if (!found) {
            Log::Error("Aligned alloc failed: could not satisfy boundary constraint %u", boundary);
            free(raw_ptr);
            return nullptr;
        }
    }

    // Setup aligned header
    AlignedSegHdr* aligned_hdr = (AlignedSegHdr*)(aligned_addr - sizeof(AlignedSegHdr));
    aligned_hdr->magic = HEAP_MAGIC_ALIGNED;
    aligned_hdr->raw_segment = raw_seg;
    aligned_hdr->user_size = size;
    aligned_hdr->alignment = alignment;
    aligned_hdr->set_guard_bytes();

    return (void*)aligned_addr;
}

void free(void* ptr) {
    if (!ptr || !heap_initialized) {
        return;
    }

    HeapSegHdr* seg = HeapSegHdr::from_data_ptr(ptr);

    if (!seg->is_valid() || seg->magic != HEAP_MAGIC_USED || seg->free) {
        Log::Error("Invalid free or double free at %p", ptr);
        return;
    }

    if (!seg->check_guard_bytes()) {
        Log::Error("Buffer overflow detected at %p", ptr);
        return;
    }

    seg->free = true;
    seg->magic = HEAP_MAGIC_FREE;
    total_freed += seg->length;

    seg->combine_forward();
    seg->combine_backward();
}

void free_aligned(void* ptr) {
    if (!ptr || !heap_initialized) {
        return;
    }

    AlignedSegHdr* aligned_hdr = (AlignedSegHdr*)((uintptr_t)ptr - sizeof(AlignedSegHdr));

    // Validate aligned header
    if (!aligned_hdr->is_valid()) {
        Log::Error("Tried to free invalid memory");
        return;
    }

    // Free the raw segment
    void* raw_ptr = aligned_hdr->raw_segment->get_data_ptr();
    free(raw_ptr);
}

void* realloc(void* ptr, size_t old_size, size_t new_size) {
    if (!heap_initialized) {
        return nullptr;
    }

    if (!ptr) {
        return malloc(new_size);
    }

    if (new_size == 0) {
        free(ptr);
        return nullptr;
    }

    new_size = align_size(new_size);
    HeapSegHdr* seg = HeapSegHdr::from_data_ptr(ptr);

    if (!seg->is_valid() || seg->magic != HEAP_MAGIC_USED || seg->free) {
        return nullptr;
    }

    // If new size fits in current segment, just return it
    if (new_size <= seg->length) {
        return ptr;
    }

    // Allocate new memory
    void* new_ptr = malloc(new_size);
    if (!new_ptr) {
        Log::Error("Realloc failed: could not allocate %u bytes", new_size);
        return nullptr;
    }

    // Copy data
    size_t copy_size = (old_size < seg->length) ? old_size : seg->length;
    copy_size = (copy_size < new_size) ? copy_size : new_size;

    memcpy(new_ptr, ptr, copy_size);
    free(ptr);

    return new_ptr;
}

void expand_heap(size_t length) {
    if (!heap_initialized) {
        Log::Error("Expand heap called before heap initialization");
        return;
    }

    // Round up to page boundary
    if (length % 0x1000) {
        length = (length + 0x1000 - 1) & ~(0x1000 - 1);
    }

    size_t page_count = length / 0x1000;
    HeapSegHdr* new_segment = (HeapSegHdr*)heap_end;

    // Map new pages
    for (size_t i = 0; i < page_count; i++) {
        kernel::memory::map_memory(heap_end, kernel::memory::request_page());
        heap_end = (void*)((size_t)heap_end + 0x1000);
    }

    // Initialize new segment
    new_segment->magic = HEAP_MAGIC_FREE;
    new_segment->free = true;
    new_segment->last = last_hdr;
    new_segment->next = nullptr;
    new_segment->length = length - HEAP_HEADER_SIZE - 1;
    new_segment->set_guard_bytes();

    // Update links
    if (last_hdr) {
        last_hdr->next = new_segment;
    }
    last_hdr = new_segment;

    Log::PrintLn("Heap expanded by %u bytes (%u pages)", length, page_count);

    // Try to combine with previous segment if it's free
    new_segment->combine_backward();
}

bool validate_heap() {
    if (!heap_initialized) return false;

    HeapSegHdr* current = (HeapSegHdr*)heap_start;
    size_t segment_count = 0;

    while (current) {
        // Prevent infinite loops
        if (++segment_count > 10000) {
            return false;
        }

        if (!current->is_valid()) {
            return false;
        }

        if (!current->check_guard_bytes()) {
            return false;
        }

        // Check forward/backward consistency
        if (current->next && current->next->last != current) {
            return false;
        }

        if (current->last && current->last->next != current) {
            return false;
        }

        current = current->next;
    }

    return true;
}

bool is_valid_pointer(void* ptr) {
    if (!ptr || !heap_initialized) return false;

    uintptr_t addr = (uintptr_t)ptr;
    if (addr < (uintptr_t)heap_start || addr >= (uintptr_t)heap_end) {
        return false;
    }

    HeapSegHdr* seg = HeapSegHdr::from_data_ptr(ptr);
    return seg->is_valid() && seg->magic == HEAP_MAGIC_USED && !seg->free;
}

size_t get_heap_usage() {
    return total_allocated - total_freed;
}

size_t get_free_space() {
    if (!heap_initialized) return 0;

    size_t free_space = 0;
    HeapSegHdr* current = (HeapSegHdr*)heap_start;

    while (current) {
        if (current->free) {
            free_space += current->length;
        }
        current = current->next;
    }

    return free_space;
}

void print_heap_stats() {
    if (!heap_initialized) return;

    Log::PrintLn("Heap Statistics:");
    Log::PrintLn("Total Allocated: %u bytes", total_allocated);
    Log::PrintLn("Total Freed: %u bytes", total_freed);
    Log::PrintLn("Current Usage: %u bytes", get_heap_usage());
    Log::PrintLn("Peak Usage: %u bytes", peak_usage);
    Log::PrintLn("Free Space: %u bytes", get_free_space());
    Log::PrintLn("Heap Range: %p - %p", heap_start, heap_end);

}