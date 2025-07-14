#ifndef CPU_MANAGER_H
#define CPU_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include "../acpi/madt.h"
#include "../memory/stack_manager.h"

#define AP_STARTUP_CODE_BASE 0x8000  // Real-Mode Code für AP-Startup

namespace CPUManager {
    
    enum CPUState {
        CPU_STATE_OFFLINE = 0,
        CPU_STATE_STARTING = 1,
        CPU_STATE_ONLINE = 2,
        CPU_STATE_HALTED = 3
    };
    
    struct CPUInfo {
        uint32_t apic_id;
        uint32_t cpu_id;
        CPUState state;
        StackManager::StackInfo* kernel_stack;
        uint64_t total_cycles;
        uint64_t idle_cycles;
        uint32_t current_task_id;
        bool is_bsp;
    };
    
    void initialize();

    void start_all_aps();
    
    bool start_ap(uint32_t apic_id);
    
    CPUInfo* get_cpu_info(uint32_t apic_id);

    uint32_t get_current_cpu_id();
    
    uint32_t get_online_cpu_count();
    
    uint32_t get_available_cpu_count();
    
    void halt_cpu(uint32_t apic_id);

    void send_ipi(uint32_t target_apic_id, uint32_t vector);
    
    void send_ipi_to_all_aps(uint32_t vector);
    
    void print_cpu_info();
    
    void update_cpu_stats(uint32_t apic_id, uint64_t cycles, uint64_t idle_cycles);
    double get_cpu_usage(uint32_t apic_id);
    
    // Externe Zugriffe für Interrupt Handler
    extern CPUInfo cpu_infos[];
    extern uint32_t total_cpus;
}

#endif // CPU_MANAGER_H
