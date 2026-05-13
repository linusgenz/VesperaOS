#ifndef CPU_MANAGER_H
#define CPU_MANAGER_H

#include <vespera/types.h>
#include <vespera/cpu/cpu_manager.h>

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

    extern CpuInfo cpu_infos[];
    extern u8 total_cpus;
}  // namespace cpu_manager

#endif  // CPU_MANAGER_H
