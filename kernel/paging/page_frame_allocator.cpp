#include "../include/page_frame_allocator.h"
#include "../include/basic_renderer.h"
#include "../../include/string.h"


PageFrameAllocator global_allocator;

void PageFrameAllocator::read_efi_memory_map(EFI_MEMORY_DESCRIPTOR* mMap, size_t mMapSize, size_t mMapDescSize){
   // if (initialized) return;
    initialized = true;

    uint64_t mMapEntries = mMapSize / mMapDescSize;

    void* largest_free_mem_seg = nullptr;
    size_t largest_free_mem_seg_size = 0;

    for (int i = 0; i < mMapEntries; i++){
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)mMap + (i * mMapDescSize));
        if (desc->type == 7){ // type = EfiConventionalMemory
            if (desc->num_pages * 4096 > largest_free_mem_seg_size)
            {
                largest_free_mem_seg = (void*)desc->phys_addr;
                largest_free_mem_seg_size = desc->num_pages * 4096;
            }
        }
    }
    uint64_t memory_size = get_memory_size(mMap, mMapEntries, mMapDescSize);
    global_renderer->new_line();
    global_renderer->print("MEMORY size:" );
    global_renderer->print(to_string(memory_size / 1024 / 1024));
    global_renderer->print("MB");
    global_renderer->new_line();
    free_memory = memory_size;
    reserved_memory = 0;
    used_memory = 0;
    uint64_t bitmapSize = memory_size / 4096 / 8 + 1;

    init_bitmap(bitmapSize, largest_free_mem_seg);

    reserve_pages(nullptr, memory_size / 4096 + 1);
    for (int i = 0; i < mMapEntries; i++){
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)mMap + (i * mMapDescSize));
        if (desc->type == 7){ // efiConventionalMemory
            unreserve_pages(desc->phys_addr, desc->num_pages);
        }
    }
    reserve_pages(nullptr, 0x100); // reserve between 0 and 0x100000
    lock_pages(page_bitmap.buffer, page_bitmap.size / 4096 + 1);
}

void PageFrameAllocator::init_bitmap(const size_t bitmap_size, void* buffer_address){
    page_bitmap.size = bitmap_size;
    page_bitmap.buffer = static_cast<uint8_t *>(buffer_address);
    for (int i = 0; i < bitmap_size; i++){
        *(uint8_t*)(page_bitmap.buffer + i) = 0;
    }
}

uint64_t page_bitmap_index = 0;
void* PageFrameAllocator::request_page(){
    for (; page_bitmap_index < page_bitmap.size * 8; page_bitmap_index++){
        if (page_bitmap[page_bitmap_index] == true) continue;
        lock_page((void*)(page_bitmap_index * 4096));
        return (void*)(page_bitmap_index * 4096);
    }

    return nullptr; // Page Frame Swap to file
}

void* PageFrameAllocator::request_pages(size_t pageCount) {
    size_t maxPages = page_bitmap.size * 8;

    for (uint64_t i = 0; i < maxPages; i++) {
        // Prüfen, ob pageCount zusammenhängende Seiten frei sind
        bool blockFree = true;
        for (size_t j = 0; j < pageCount; j++) {
            if ((i + j) >= maxPages || page_bitmap[i + j]) {
                blockFree = false;
                i += j; // nächster Versuch fängt dort an, wo's aufgehört hat
                break;
            }
        }

        if (blockFree) {
            for (size_t j = 0; j < pageCount; j++) {
                lock_page((void*)((i + j) * 4096));
            }
            return (void*)(i * 4096);
        }
    }

    return nullptr; // nichts gefunden
}

void PageFrameAllocator::free_page(void* address){
    uint64_t index = (uint64_t)address / 4096;
    if (page_bitmap[index] == false) return;
    if (page_bitmap.set(index, false)) {
        free_memory += 4096;
        used_memory -= 4096;
        if (page_bitmap_index > index) page_bitmap_index = index;
    }
}

void PageFrameAllocator::free_pages(void* address, uint64_t page_count){
    for (int t = 0; t < page_count; t++){
        free_page((void*)((uint64_t)address + (t * 4096)));
    }
}

void PageFrameAllocator::lock_page(void* address){
    uint64_t index = (uint64_t)address / 4096;
    if (page_bitmap[index] == true) return;
    if (page_bitmap.set(index, true)) {
        free_memory -= 4096;
        used_memory += 4096;
    }
}

void PageFrameAllocator::lock_pages(void* address, uint64_t page_count){
    for (int t = 0; t < page_count; t++){
        lock_page((void*)((uint64_t)address + (t * 4096)));
    }
}

void PageFrameAllocator::unreserve_page(uint64_t address){
    uint64_t index = address / 4096;
    if (page_bitmap[index] == false) return;
    if (page_bitmap.set(index, false)) {
        free_memory += 4096;
        reserved_memory -= 4096;
        if (page_bitmap_index > index) page_bitmap_index = index;
    }
}

void PageFrameAllocator::unreserve_pages(uint64_t address, uint64_t page_count){
    for (int t = 0; t < page_count; t++){
        unreserve_page(address + (t * 4096));
    }
}

void PageFrameAllocator::reserve_page(void* address){
    uint64_t index = (uint64_t)address / 4096;
    if (page_bitmap[index] == true) return;
    if (page_bitmap.set(index, true)) {
        free_memory -= 4096;
        reserved_memory += 4096;
    }
}

void PageFrameAllocator::reserve_pages(void* address, uint64_t page_count){
    for (int t = 0; t < page_count; t++){
        reserve_page((void*)((uint64_t)address + (t * 4096)));
    }
}

uint64_t PageFrameAllocator::get_free_ram(){
    return free_memory;
}
uint64_t PageFrameAllocator::get_used_ram(){
    return used_memory;
}
uint64_t PageFrameAllocator::get_reserved_ram(){
    return reserved_memory;
}