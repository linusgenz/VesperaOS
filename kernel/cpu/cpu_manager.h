#ifndef CPU_MANAGER_H
#define CPU_MANAGER_H

#include <vespera/types.h>

#define KERNEL_STACK_BASE 0x20000
#define KERNEL_STACK_SIZE 0x1000
#define CPU_ID_REG 0x6008
#define CPU_READY_REG 0x600C
#define SIPI_VECTOR 0x8
extern volatile u8 g_active_cpu_count;

struct __attribute__((packed)) CpuStartupReport {
    u32 apic_id;
    u32 rsv0;
    u64 stack_pointer;
    bool ready;
    bool go;
    u8 rsv1[6];
};

#define CPU_STARTUP_REPORTS ((CpuStartupReport*)0x7000)

namespace cpu_manager {

    enum CpuState { CPU_STATE_OFFLINE = 0, CPU_STATE_STARTING = 1, CPU_STATE_ONLINE = 2, CPU_STATE_HALTED = 3 };

    struct CpuAccounting {
        u64 last_tick_tsc;
        u64 total_cycles;
        u64 idle_cycles;
    };

    struct CpuInfo {
        u32 apic_id;
        u32 cpu_id;
        CpuState state;
        // StackManager::StackInfo* kernel_stack;
        uptr kernel_stack;
        uptr kernel_stack_top;
        CpuAccounting accounting;
        u32 current_task_id;
        bool is_bsp;
    };

    void initialize();

    void smp_init();
    void init_core(const CpuInfo* cpu);

    CpuInfo* get_cpu_info(u32 apic_id);

    u8 get_current_cpu_id();

    u8 get_online_cpu_count();

    u8 get_available_cpu_count();

    void halt_cpu(u32 apic_id);

    void send_ipi_to_all_aps(u32 vector);

    void print_cpu_info();

    void accounting_tick(const u32 cpu_id, const bool is_idle);
    int get_cpu_usage_percent(const u32 cpu_id);

    extern CpuInfo cpu_infos[];
    extern u8 total_cpus;
}  // namespace CPUManager

#endif  // CPU_MANAGER_H
