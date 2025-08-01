#include "stack_manager.h"
#include "../include/memory.h"
#include "../../include/log.h"
#include "heap.h"

namespace StackManager {
    
    static StackInfo stacks[MAX_STACKS];
    static uint32_t stack_count = 0;
    static bool is_initialized = false;
    
    void initialize() {
        if (is_initialized) return;
        
        // Alle Stacks als nicht allokiert markieren
        for (uint32_t i = 0; i < MAX_STACKS; i++) {
            stacks[i].stack_base = nullptr;
            stacks[i].stack_top = nullptr;
            stacks[i].stack_size = 0;
            stacks[i].cpu_id = 0;
            stacks[i].is_allocated = false;
            stacks[i].is_kernel_stack = false;
        }
        
        stack_count = 0;
        is_initialized = true;
        
        Log::Ok("Stack Manager initialized");
    }
    
    StackInfo* allocate_kernel_stack(uint32_t cpu_id) {
        if (!is_initialized) initialize();
        
        for (uint32_t i = 0; i < stack_count; i++) {
            if (stacks[i].is_allocated && stacks[i].cpu_id == cpu_id) {
                Log::Warning("Stack for CPU %u already allocated", cpu_id);
                return &stacks[i];
            }
        }
        
        if (stack_count >= MAX_STACKS) {
            Log::Error("Cannot allocate more stacks - maximum reached");
            return nullptr;
        }
        
        size_t pages_needed = (KERNEL_STACK_SIZE + 0xFFF) / 0x1000;
        
        void* stack_base = kernel::memory::request_pages(pages_needed);
        if (!stack_base) {
            Log::Error("Failed to allocate memory for kernel stack");
            return nullptr;
        }
        
        for (size_t i = 0; i < pages_needed; i++) {
            void* virt_addr = (void*)((uint64_t)stack_base + (i * 0x1000));
            void* phys_addr = (void*)((uint64_t)stack_base + (i * 0x1000));
            kernel::memory::map_memory(virt_addr, phys_addr, (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));
        }
        
        // Initialisiere Stack-Info
        StackInfo* stack_info = &stacks[stack_count];
        stack_info->stack_base = stack_base;
        stack_info->stack_top = (void*)((uint64_t)stack_base + KERNEL_STACK_SIZE);
        stack_info->stack_size = KERNEL_STACK_SIZE;
        stack_info->cpu_id = cpu_id;
        stack_info->is_allocated = true;
        stack_info->is_kernel_stack = true;
        
        // Stack mit einem Pattern initialisieren (für Debug)
        memset(stack_base, 0xCC, KERNEL_STACK_SIZE);
        
        stack_count++;
        
        Log::Info("Allocated kernel stack for CPU %u: base=%p, top=%p, size=%u",
                  cpu_id, stack_base, stack_info->stack_top, KERNEL_STACK_SIZE);
        
        return stack_info;
    }
    
    StackInfo* allocate_user_stack(size_t stack_size) {
        if (!is_initialized) initialize();
        
        if (stack_count >= MAX_STACKS) {
            Log::Error("Cannot allocate more stacks - maximum reached");
            return nullptr;
        }
        
        // Runde auf Page-Boundary auf
        if (stack_size % 0x1000 != 0) {
            stack_size = (stack_size + 0xFFF) & ~0xFFF;
        }
        
        size_t pages_needed = stack_size / 0x1000;
        
        // Allokiere Pages für den Stack
        void* stack_base = kernel::memory::request_pages(pages_needed);
        if (!stack_base) {
            Log::Error("Failed to allocate memory for user stack");
            return nullptr;
        }
        
        // Mappe die Pages in den virtuellen Adressraum
        for (size_t i = 0; i < pages_needed; i++) {
            void* virt_addr = (void*)((uint64_t)stack_base + (i * 0x1000));
            void* phys_addr = (void*)((uint64_t)stack_base + (i * 0x1000));
            kernel::memory::map_memory(virt_addr, phys_addr, (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));
        }
        
        // Initialisiere Stack-Info
        StackInfo* stack_info = &stacks[stack_count];
        stack_info->stack_base = stack_base;
        stack_info->stack_top = (void*)((uint64_t)stack_base + stack_size);
        stack_info->stack_size = stack_size;
        stack_info->cpu_id = 0; // User-Stacks haben keine feste CPU-ID
        stack_info->is_allocated = true;
        stack_info->is_kernel_stack = false;
        
        // Stack mit einem Pattern initialisieren (für Debug)
        memset(stack_base, 0xAA, stack_size);
        
        stack_count++;
        
        Log::Info("Allocated user stack: base=0x%p, top=0x%p, size=%u", 
                  stack_base, stack_info->stack_top, stack_size);
        
        return stack_info;
    }
    
    void free_stack(StackInfo* stack_info) {
        if (!stack_info || !stack_info->is_allocated) {
            Log::Warning("Attempted to free invalid stack");
            return;
        }
        
        // Berechne die Anzahl der Pages
        size_t pages_needed = (stack_info->stack_size + 0xFFF) / 0x1000;
        
        // TODO: Unmap Pages (optional - für jetzt nicht kritisch)
        // for (size_t i = 0; i < pages_needed; i++) {
        //     void* virt_addr = (void*)((uint64_t)stack_info->stack_base + (i * 0x1000));
        //     global_page_table_manager.unmap_memory(virt_addr);
        // }
        
        // Gebe Pages frei
        kernel::memory::free_pages(stack_info->stack_base, pages_needed);
        
        // Markiere Stack als frei
        stack_info->stack_base = nullptr;
        stack_info->stack_top = nullptr;
        stack_info->stack_size = 0;
        stack_info->cpu_id = 0;
        stack_info->is_allocated = false;
        stack_info->is_kernel_stack = false;
        
        Log::Info("Freed stack");
    }
    
    StackInfo* get_stack_for_cpu(uint32_t cpu_id) {
        if (!is_initialized) return nullptr;
        
        for (uint32_t i = 0; i < stack_count; i++) {
            if (stacks[i].is_allocated && stacks[i].cpu_id == cpu_id && stacks[i].is_kernel_stack) {
                return &stacks[i];
            }
        }
        
        return nullptr;
    }
    
    void* get_current_stack_pointer() {
        void* rsp;
        asm volatile("mov %%rsp, %0" : "=r"(rsp));
        return rsp;
    }
    
    void set_stack_pointer(void* stack_top) {
        asm volatile("mov %0, %%rsp" : : "r"(stack_top) : "memory");
    }
    
    void setup_stack_guard_pages(StackInfo* stack_info) {
        if (!stack_info || !stack_info->is_allocated) return;
        
        // Erstelle eine Guard-Page am unteren Ende des Stacks
        void* guard_page = (void*)((uint64_t)stack_info->stack_base - 0x1000);
        
        // Allokiere eine Page für den Guard
        void* guard_phys = kernel::memory::request_page();
        if (!guard_phys) {
            Log::Warning("Failed to allocate guard page");
            return;
        }
        
        // Mappe die Guard-Page als read-only (keine write-permission)
        kernel::memory::map_memory(guard_page, guard_phys);
        
        Log::Info("Setup stack guard page at 0x%p", guard_page);
    }
    
    void print_stack_info() {
        if (!is_initialized) {
            Log::Warning("Stack Manager not initialized");
            return;
        }
        
        Log::Info("=== Stack Manager Info ===");
        Log::Info("Total stacks: %u/%u", stack_count, MAX_STACKS);
        
        for (uint32_t i = 0; i < stack_count; i++) {
            if (stacks[i].is_allocated) {
                Log::Info("Stack %u: CPU %u, %s, base=%p, top=%p, size=%u",
                          i, 
                          stacks[i].cpu_id,
                          stacks[i].is_kernel_stack ? "kernel" : "user",
                          stacks[i].stack_base,
                          stacks[i].stack_top,
                          stacks[i].stack_size);
            }
        }
    }
    
    size_t get_stack_usage(StackInfo* stack_info) {
        if (!stack_info || !stack_info->is_allocated) return 0;
        
        // Einfache Heuristik: Schaue, wie viele Bytes am Anfang noch das Debug-Pattern haben
        uint8_t* stack_ptr = (uint8_t*)stack_info->stack_base;
        uint8_t pattern = stack_info->is_kernel_stack ? 0xCC : 0xAA;
        
        size_t unused_bytes = 0;
        for (size_t i = 0; i < stack_info->stack_size; i++) {
            if (stack_ptr[i] == pattern) {
                unused_bytes++;
            } else {
                break;
            }
        }
        
        return stack_info->stack_size - unused_bytes;
    }
}
