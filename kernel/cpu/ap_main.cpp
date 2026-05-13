//
// Created by Linus on 17.07.25.
//
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/apic.h>
#include <arch/x86_64/syscall.h>
#include <vespera/kerrno.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>

#include "../scheduling/per_cpu.h"
#include "cpu_manager.h"
#include "vespera/cpu/simd.h"

extern "C" void ap_main() {
    simd_enable_on_current_core();

    const u32 cpu_id = cpu_manager::get_current_cpu_id();

    if (!cpu_id) {
        kernel::SystemManager::system_panic("Failed to find CPU ID", -KENOCPUID);
    }

    load_gdt();

    setup_cpu_tss(cpu_id);

    syscall_init();

    per_cpu_init(cpu_id);

    arch::x86_64::interrupts::apic::init(cpu_id);

    kernel::scheduling::enable_on_cpu(cpu_id);

    kernel::SystemManager::system_panic("AP core returned from context switch", -KEAPRETURN);
}
