//
// Created by Linus on 17.07.25.
//
#include <vespera/interrupts.h>
#include <vespera/kerrno.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>

#include "../../arch/x86_64/gdt/gdt.h"
#include "../../arch/x86_64/syscalls/syscall.h"
#include "cpu_manager.h"

extern "C" void ap_main() {
    const uint32_t cpu_id = cpu_manager::get_current_cpu_id();

    if (!cpu_id) {
        kernel::SystemManager::system_panic("Failed to find CPU ID", -KENOCPUID);
    }

    load_gdt(&gdt_ptr);

    setup_cpu_tss(cpu_id);
    syscall_init();

    kernel::interrupts::lapic_init(cpu_id);

    kernel::scheduling::enable_on_cpu(cpu_id);

    kernel::SystemManager::system_panic("AP core returned from context switch", -KEAPRETURN);
}
