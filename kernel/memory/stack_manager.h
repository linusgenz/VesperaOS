#ifndef STACK_MANAGER_H
#define STACK_MANAGER_H

#include <cstdint>
#include <cstddef>

#define KERNEL_STACK_SIZE 16384  // 16KB per stack
#define MAX_STACKS 64            // max 64 stacks (max 64 cores)

namespace StackManager {
    
    struct StackInfo {
        void* stack_base;        // Basis-Adresse des Stacks
        void* stack_top;         // Top-Adresse des Stacks (für RSP)
        size_t stack_size;       // Größe des Stacks
        uint32_t cpu_id;         // CPU-ID, die diesen Stack nutzt
        bool is_allocated;       // Ob der Stack allokiert ist
        bool is_kernel_stack;    // Kernel-Stack oder User-Stack
    };
    
    void initialize();
    
    StackInfo* allocate_kernel_stack(uint32_t cpu_id);
    
    StackInfo* allocate_user_stack(size_t stack_size = 8192);
    
    void free_stack(StackInfo* stack_info);
    
    StackInfo* get_stack_for_cpu(uint32_t cpu_id);
    
    void* get_current_stack_pointer();
    
    void set_stack_pointer(void* stack_top);

    void setup_stack_guard_pages(StackInfo* stack_info);
    
    void print_stack_info();
    size_t get_stack_usage(StackInfo* stack_info);
}

#endif // STACK_MANAGER_H
