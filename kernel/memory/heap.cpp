#include "heap.h"
#include <kernel/memory.h>
#include <log.h>

#include "../cpu/io.h"

void* heap_start = nullptr;
void* heap_end = nullptr;
HeapSegHdr* last_hdr = nullptr;
bool heap_initialized = false;

// Statistics
size_t total_allocated = 0;
size_t total_freed = 0;
size_t peak_usage = 0;

static inline uintptr_t align_up(uintptr_t x, size_t a)
{
    return (x + a - 1) & ~(a - 1);
}

bool HeapSegHdr::is_valid() const
{
    return (magic == HEAP_MAGIC_FREE || magic == HEAP_MAGIC_USED) &&
        length > 0 && length < 0x100000000ULL;
}

void HeapSegHdr::set_guard_bytes()
{
    guard_start = HEAP_GUARD_PATTERN;

    // FIX: End-Guard nur setzen wenn kein Nachbar-Segment direkt anschließt.
    // Ohne diesen Guard liegt end_guard auf new_seg->magic[0] und überschreibt
    // es mit 0xAA → is_valid() schlägt fehl → Heap-Corruption.
    if (!next)
    {
        auto* end_guard = reinterpret_cast<uint8_t*>(
            reinterpret_cast<uintptr_t>(this) + HEAP_HEADER_SIZE + length);
        if (reinterpret_cast<uintptr_t>(end_guard) < reinterpret_cast<uintptr_t>(heap_end))
        {
            *end_guard = HEAP_GUARD_PATTERN;
        }
    }

    // Checksum immer aktualisieren (deckt magic + length + next-Pointer ab)
    checksum = magic
        ^ static_cast<uint32_t>(length)
        ^ static_cast<uint32_t>(reinterpret_cast<uintptr_t>(next) >> 32)
        ^ static_cast<uint32_t>(reinterpret_cast<uintptr_t>(next));
}

bool HeapSegHdr::check_guard_bytes() const
{
    if (guard_start != HEAP_GUARD_PATTERN)
        return false;

    // Checksum verifizieren
    const auto expected_checksum = magic
        ^ static_cast<uint32_t>(length)
        ^ static_cast<uint32_t>(reinterpret_cast<uintptr_t>(next) >> 32)
        ^ static_cast<uint32_t>(reinterpret_cast<uintptr_t>(next));
    if (checksum != expected_checksum)
        return false;

    // FIX: End-Guard nur prüfen wenn kein direkt anschließendes Segment folgt
    // (symmetrisch zu set_guard_bytes)
    if (!next)
    {
        if (auto* end_guard = reinterpret_cast<const uint8_t*>(
                reinterpret_cast<uintptr_t>(this) + HEAP_HEADER_SIZE + length);
            reinterpret_cast<uintptr_t>(end_guard) < reinterpret_cast<uintptr_t>(heap_end))
        {
            return *end_guard == HEAP_GUARD_PATTERN;
        }
    }

    return true;
}


HeapSegHdr* HeapSegHdr::split(const size_t split_length)
{
    if (!is_valid())
    {
        Log::Error("Split failed: invalid segment header at %p", this);
        return nullptr;
    }
    if (!free)
    {
        Log::Error("Split failed: can only split free segments");
        return nullptr;
    }
    if (split_length < MIN_ALLOC_SIZE)
    {
        Log::Error("Split failed: split length %zu too small (min: %u)", split_length, MIN_ALLOC_SIZE);
        return nullptr;
    }

    if (length < split_length + HEAP_HEADER_SIZE + MIN_ALLOC_SIZE)
    {
        Log::debug("Split not possible: remaining would be too small (seg len=%zu, req=%zu)", length, split_length);
        return nullptr;
    }

    // Neue Header-Adresse: nach Nutzerdaten, aufgerundet auf MIN_ALIGNMENT.
    // Das stellt sicher dass get_data_ptr() des neuen Segments 16-Byte-aligned ist
    // und rep-stosq (Compiler-Null-Init) nicht mit GPF abstürzt.
    uintptr_t new_seg_addr =
        align_up(
            (uintptr_t)this + HEAP_HEADER_SIZE + split_length,
            MIN_ALIGNMENT
        );
    auto* new_seg = reinterpret_cast<HeapSegHdr*>(new_seg_addr);

    // Verbleibende Nutzerdaten-Länge für das neue Segment
    size_t remaining_length =
        (uintptr_t)this + HEAP_HEADER_SIZE + this->length
        - new_seg_addr
        - HEAP_HEADER_SIZE;

    if (remaining_length < MIN_ALLOC_SIZE)
    {
        Log::Error("Split failed: remaining length %zu too small after split", remaining_length);
        return nullptr;
    }

    new_seg->magic = HEAP_MAGIC_FREE;
    new_seg->length = remaining_length;
    new_seg->free = true;
    new_seg->last = this;
    new_seg->next = this->next;
    new_seg->set_guard_bytes();

    if (new_seg->next)
    {
        new_seg->next->last = new_seg;
    }

    // Tatsächliche Länge dieses Segments: Abstand bis zum neuen Header minus
    // eigener Header-Größe (berücksichtigt Alignment-Padding)
    this->length = new_seg_addr - (uintptr_t)this - HEAP_HEADER_SIZE;
    this->next = new_seg;
    this->set_guard_bytes();

    if (last_hdr == this)
    {
        last_hdr = new_seg;
    }

    return new_seg;
}


void HeapSegHdr::combine_forward()
{
    if (!next || !next->free || !is_valid() || !next->is_valid())
    {
        return;
    }

    if (next == last_hdr)
    {
        last_hdr = this;
    }

    this->length += HEAP_HEADER_SIZE + next->length;

    HeapSegHdr* after = next->next;
    this->next = after;
    if (after)
    {
        after->last = this;
    }

    this->set_guard_bytes();
}

void HeapSegHdr::combine_backward() const
{
    if (last && last->free && last->is_valid())
    {
        last->combine_forward();
    }
}


bool initialize_heap(void* heap_address, size_t page_count)
{
    if (heap_initialized)
    {
        Log::Error("Heap already initialized");
        return false;
    }
    if (!heap_address)
    {
        Log::Error("Invalid heap address (null pointer)");
        return false;
    }
    if (page_count == 0)
    {
        Log::Error("Invalid page count (zero pages)");
        return false;
    }

    void* pos = heap_address;
    outb(0x3F8, 'H');

    // Seiten mappen
    for (size_t i = 0; i < page_count; i++)
    {
        kernel::memory::map_memory(pos, (void*)kernel::memory::request_page_phys());
        memset(pos, 0, PAGE_SIZE);
        pos = reinterpret_cast<void*>(reinterpret_cast<size_t>(pos) + 0x1000);
    }

    size_t heap_length = page_count * 0x1000;

    uintptr_t aligned_start = align_up((uintptr_t)heap_address, MIN_ALIGNMENT);
    size_t adjustment = aligned_start - (uintptr_t)heap_address;

    heap_start = (void*)aligned_start;
    heap_end   = (void*)((uintptr_t)heap_address + heap_length);
    heap_length -= adjustment;

    auto* start_seg = static_cast<HeapSegHdr*>((void*)aligned_start);
    start_seg->magic = HEAP_MAGIC_FREE;
    start_seg->length = heap_length - HEAP_HEADER_SIZE;
    start_seg->next = nullptr;
    start_seg->last = nullptr;
    start_seg->free = true;
    start_seg->set_guard_bytes();

    last_hdr = start_seg;
    heap_initialized = true;

    total_allocated = 0;
    total_freed = 0;
    peak_usage = 0;

    return true;
}

size_t align_size(size_t size)
{
    if (size == 0) return MIN_ALLOC_SIZE;

    if (size % MIN_ALIGNMENT != 0)
    {
        size = (size + MIN_ALIGNMENT - 1) & ~(MIN_ALIGNMENT - 1);
    }

    return size < MIN_ALLOC_SIZE ? MIN_ALLOC_SIZE : size;
}

HeapSegHdr* find_free_segment(size_t size)
{
    if (!heap_initialized) return nullptr;

    auto* current_seg = static_cast<HeapSegHdr*>(heap_start);

    while (current_seg)
    {
        if (!current_seg->is_valid())
        {
            Log::Error("Heap segment is not valid");
            asm volatile("cli; hlt");
            return nullptr;
        }

        if (current_seg->free && current_seg->length >= size)
        {
            return current_seg;
        }

        current_seg = current_seg->next;
    }

    return nullptr;
}

void* allocate_from_segment(HeapSegHdr* seg, size_t size)
{
    if (!seg || !seg->free || seg->length < size)
    {
        return nullptr;
    }

    if (seg->length >= size + HEAP_HEADER_SIZE + MIN_ALLOC_SIZE)
    {
        HeapSegHdr* new_seg = seg->split(size);
        if (!new_seg)
        {
            Log::Warning("Split failed, taking whole segment");
        }
    }

    seg->free = false;
    seg->magic = HEAP_MAGIC_USED;
    seg->set_guard_bytes();

    total_allocated += seg->length;
    if (total_allocated - total_freed > peak_usage)
    {
        peak_usage = total_allocated - total_freed;
    }

    return seg->get_data_ptr();
}


void* malloc(size_t size)
{
    if (!heap_initialized || size == 0)
    {
        return nullptr;
    }

    size = align_size(size);

    HeapSegHdr* seg = find_free_segment(size);
    if (seg)
    {
        return allocate_from_segment(seg, size);
    }

    expand_heap(size);
    seg = find_free_segment(size);
    if (seg)
    {
        return allocate_from_segment(seg, size);
    }

    return nullptr;
}

void* alloc_aligned(size_t size, size_t alignment, size_t boundary)
{
    if (!heap_initialized || size == 0 || alignment == 0)
    {
        return nullptr;
    }

    if ((alignment & (alignment - 1)) != 0)
    {
        Log::Error("Alignment must be a power of 2: %u", alignment);
        return nullptr;
    }

    if (boundary != 0 && (boundary & (boundary - 1)) != 0)
    {
        Log::Error("Bounds must be a power of 2");
        return nullptr;
    }

    if (alignment < MIN_ALIGNMENT)
    {
        alignment = MIN_ALIGNMENT;
    }

    size = align_size(size);

    size_t header_size = sizeof(AlignedSegHdr);
    size_t max_padding = alignment - 1 + header_size;
    size_t total_size = size + max_padding;

    if (boundary > 0)
    {
        total_size += boundary;
    }

    void* raw_ptr = malloc(total_size);
    if (!raw_ptr)
    {
        Log::Error("Aligned alloc failed: could not allocate %u bytes for aligned allocation", total_size);
        return nullptr;
    }

    HeapSegHdr* raw_seg = HeapSegHdr::from_data_ptr(raw_ptr);

    auto raw_addr = reinterpret_cast<uintptr_t>(raw_ptr);
    uintptr_t aligned_addr = (raw_addr + header_size + alignment - 1) & ~(alignment - 1);

    if (boundary > 0)
    {
        const uintptr_t end_addr = raw_addr + total_size;

        bool found = false;
        for (uintptr_t candidate = aligned_addr;
             candidate + size <= end_addr;
             candidate += alignment)
        {
            uintptr_t start_boundary = candidate & ~(boundary - 1);
            uintptr_t end_boundary = (candidate + size - 1) & ~(boundary - 1);

            if (start_boundary == end_boundary)
            {
                aligned_addr = candidate;
                found = true;
                break;
            }
        }

        if (!found)
        {
            Log::Error("Aligned alloc failed: could not satisfy boundary constraint %u", boundary);
            free(raw_ptr);
            return nullptr;
        }
    }

    auto* aligned_hdr = reinterpret_cast<AlignedSegHdr*>(aligned_addr - sizeof(AlignedSegHdr));
    aligned_hdr->magic = HEAP_MAGIC_ALIGNED;
    aligned_hdr->raw_segment = raw_seg;
    aligned_hdr->user_size = size;
    aligned_hdr->alignment = alignment;
    aligned_hdr->set_guard_bytes();

    return reinterpret_cast<void*>(aligned_addr);
}

void free(void* ptr)
{
    if (!ptr || !heap_initialized)
    {
        return;
    }

    HeapSegHdr* seg = HeapSegHdr::from_data_ptr(ptr);

    if (!seg->is_valid() || seg->magic != HEAP_MAGIC_USED || seg->free)
    {
        Log::Error("Invalid free or double free at %p", ptr);
        return;
    }

    if (!seg->check_guard_bytes())
    {
        Log::Error("Buffer overflow detected at %p", ptr);
        return;
    }

    seg->free = true;
    seg->magic = HEAP_MAGIC_FREE;
    // FIX: Checksum nach magic-Wechsel neu berechnen.
    // Ohne diesen Aufruf enthält checksum noch den HEAP_MAGIC_USED-Wert;
    // check_guard_bytes() schlägt dann bei validate_heap() fehl.
    seg->set_guard_bytes();

    total_freed += seg->length;

    seg->combine_forward();
    seg->combine_backward();
}

void free_aligned(void* ptr)
{
    if (!ptr || !heap_initialized)
    {
        return;
    }

    auto* aligned_hdr = reinterpret_cast<AlignedSegHdr*>(reinterpret_cast<uintptr_t>(ptr) - sizeof(AlignedSegHdr));

    if (!aligned_hdr->is_valid())
    {
        Log::Error("Tried to free invalid memory");
        return;
    }

    void* raw_ptr = aligned_hdr->raw_segment->get_data_ptr();
    free(raw_ptr);
}

void* realloc(void* ptr, const size_t old_size, size_t new_size)
{
    if (!heap_initialized)
    {
        return nullptr;
    }

    if (!ptr)
    {
        return malloc(new_size);
    }

    if (new_size == 0)
    {
        free(ptr);
        return nullptr;
    }

    new_size = align_size(new_size);
    const HeapSegHdr* seg = HeapSegHdr::from_data_ptr(ptr);

    if (!seg->is_valid() || seg->magic != HEAP_MAGIC_USED || seg->free)
    {
        return nullptr;
    }

    if (new_size <= seg->length)
    {
        return ptr;
    }

    void* new_ptr = malloc(new_size);
    if (!new_ptr)
    {
        Log::Error("Realloc failed: could not allocate %u bytes", new_size);
        return nullptr;
    }

    size_t copy_size = (old_size < seg->length) ? old_size : seg->length;
    copy_size = (copy_size < new_size) ? copy_size : new_size;

    memcpy(new_ptr, ptr, copy_size);
    free(ptr);

    return new_ptr;
}

void expand_heap(size_t length)
{
    if (!heap_initialized)
    {
        Log::Error("Expand heap called before heap initialization");
        return;
    }

    if (length % 0x1000)
    {
        length = (length + 0x1000 - 1) & ~(0x1000 - 1);
    }

    size_t page_count = length / 0x1000;
    auto* new_segment = static_cast<HeapSegHdr*>(heap_end);

    for (size_t i = 0; i < page_count; i++)
    {
        void* virt = heap_end;
        void* phys = kernel::memory::request_page();
        if (!phys)
        {
            length = i * 0x1000;
            break;
        }
        kernel::memory::map_memory(virt, phys);
        memset(virt, 0, PAGE_SIZE);
        heap_end = reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(heap_end) + 0x1000
        );
    }

    new_segment->magic = HEAP_MAGIC_FREE;
    new_segment->free = true;
    new_segment->last = last_hdr;
    new_segment->next = nullptr;
    new_segment->length = length - HEAP_HEADER_SIZE - 1;
    new_segment->set_guard_bytes();

    if (last_hdr)
    {
        last_hdr->next = new_segment;
    }
    last_hdr = new_segment;

    Log::PrintLn("Heap expanded by %u bytes (%u pages)", length, page_count);

    new_segment->combine_backward();
}

bool validate_heap()
{
    if (!heap_initialized) return false;

    const auto* current = static_cast<HeapSegHdr*>(heap_start);
    size_t segment_count = 0;

    while (current)
    {
        if (++segment_count > 10000)
        {
            return false;
        }

        if (!current->is_valid())
        {
            return false;
        }

        if (!current->check_guard_bytes())
        {
            return false;
        }

        if (current->next && current->next->last != current)
        {
            return false;
        }

        if (current->last && current->last->next != current)
        {
            return false;
        }

        current = current->next;
    }

    return true;
}

bool is_valid_pointer(void* ptr)
{
    if (!ptr || !heap_initialized) return false;

    if (const auto addr = reinterpret_cast<uintptr_t>(ptr); addr < reinterpret_cast<uintptr_t>(heap_start) || addr >=
        reinterpret_cast<uintptr_t>(heap_end))
    {
        return false;
    }

    const HeapSegHdr* seg = HeapSegHdr::from_data_ptr(ptr);
    return seg->is_valid() && seg->magic == HEAP_MAGIC_USED && !seg->free;
}

size_t get_heap_usage()
{
    return total_allocated - total_freed;
}

size_t get_free_space()
{
    if (!heap_initialized) return 0;

    size_t free_space = 0;
    const auto* current = static_cast<HeapSegHdr*>(heap_start);

    while (current)
    {
        if (current->free)
        {
            free_space += current->length;
        }
        current = current->next;
    }

    return free_space;
}

void print_heap_stats()
{
    if (!heap_initialized) return;

    Log::PrintLn("Heap Statistics:");
    Log::PrintLn("Total Allocated: %u bytes", total_allocated);
    Log::PrintLn("Total Freed: %u bytes", total_freed);
    Log::PrintLn("Current Usage: %u bytes", get_heap_usage());
    Log::PrintLn("Peak Usage: %u bytes", peak_usage);
    Log::PrintLn("Free Space: %u bytes", get_free_space());
    Log::PrintLn("Heap Range: %p - %p", heap_start, heap_end);
}