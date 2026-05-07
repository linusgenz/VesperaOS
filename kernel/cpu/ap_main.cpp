//
// Created by Linus on 17.07.25.
//
#include <vespera/interrupts.h>
#include <vespera/kerrno.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>

#include "../../arch/x86_64/gdt/gdt.h"
#include "../../arch/x86_64/syscalls/syscall.h"
#include "../scheduling/per_cpu.h"
#include "cpu_manager.h"
#include "vespera/cpu/simd.h"
#include "vespera/log.h"

static inline void enable_fsgsbase() {
    u64 cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 16); // FSGSBASE
    asm volatile("mov %0, %%cr4" :: "r"(cr4));
}

extern "C" void ap_main() {
    simd_enable_on_current_core();
    enable_fsgsbase();
    const u32 cpu_id = cpu_manager::get_current_cpu_id();

    if (!cpu_id) {
        kernel::SystemManager::system_panic("Failed to find CPU ID", -KENOCPUID);
    }

    load_gdt(&gdt_ptr);

    setup_cpu_tss(cpu_id);

    syscall_init();

    per_cpu_init(cpu_id);

    kernel::interrupts::lapic_init(cpu_id);

    kernel::scheduling::enable_on_cpu(cpu_id);

    kernel::SystemManager::system_panic("AP core returned from context switch", -KEAPRETURN);
}
