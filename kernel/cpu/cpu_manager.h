#ifndef CPU_MANAGER_H
#define CPU_MANAGER_H

#include "../acpi/madt.h"
#include "../memory/stack_manager.h"
#include "kernel/sync/completion.h"
#include <cstddef>
#include <cstdint>

#define KERNEL_STACK_BASE 0x20000
#define KERNEL_STACK_SIZE 0x1000
#define CPU_ID_REG 0x6008
#define CPU_READY_REG 0x600C
#define SIPI_VECTOR 0x8
extern volatile uint8_t g_activeCpuCount;

struct __attribute__((packed)) CpuStartupReport {
    uint32_t apic_id;
    uint32_t rsv0;
    uint64_t stack_pointer;
    bool ready;
    bool go;
    uint8_t rsv1[6];
};

#define cpu_startup_reports ((CpuStartupReport*)0x7000)

namespace CPUManager {

    enum CPUState { CPU_STATE_OFFLINE = 0, CPU_STATE_STARTING = 1, CPU_STATE_ONLINE = 2, CPU_STATE_HALTED = 3 };

    struct CPUInfo {
        uint32_t apic_id;
        uint32_t cpu_id;
        CPUState state;
        // StackManager::StackInfo* kernel_stack;
        uintptr_t kernel_stack;
        uintptr_t kernel_stack_top;
        uint64_t total_cycles;
        uint64_t idle_cycles;
        uint32_t current_task_id;
        bool is_bsp;
    };

    void initialize();

    void smp_init();
    void init_core(const CPUInfo* cpu);

    CPUInfo* get_cpu_info(uint32_t apic_id);

    uint8_t get_current_cpu_id();

    uint8_t get_online_cpu_count();

    uint8_t get_available_cpu_count();

    void halt_cpu(uint32_t apic_id);

    void send_ipi_to_all_aps(uint32_t vector);

    void print_cpu_info();

    void update_cpu_stats(uint32_t apic_id, uint64_t cycles, uint64_t idle_cycles);
    int get_cpu_usage(uint32_t apic_id);

    // Externe Zugriffe für Interrupt Handler
    extern CPUInfo cpu_infos[];
    extern uint8_t total_cpus;
}  // namespace CPUManager

#endif  // CPU_MANAGER_H
