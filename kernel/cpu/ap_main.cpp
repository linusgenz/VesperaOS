//
// Created by Linus on 17.07.25.
//
#include "cpu_manager.h"
#include <log.h>
#include <kernel/system/system_manager.h>
#include <kernel/interrupts.h>
#include <kernel/kerrno.h>
#include <kernel/scheduling.h>
#include "../../arch/x86_64/gdt/gdt.h"
#include "../../arch/x86_64/syscalls/syscall.h"
#include "kernel/time.h"

extern "C" void ap_main(uint32_t apic_id)
{
    const uint32_t cpu_id = CPUManager::get_current_cpu_id();

    if (!cpu_id)
    {
        kernel::SystemManager::system_panic("Failed to find CPU ID", -KENOCPUID);
    }

    load_GDT(&gdt_ptr);

    setup_cpu_tss(cpu_id);
    syscall_init();

    kernel::interrupts::lapic_init(cpu_id);

    Log::Ok("Cpu %u initialized", cpu_id);

    kernel::scheduling::enable_on_cpu(cpu_id);

    kernel::SystemManager::system_panic("AP core returned from context switch", -KEAPRETURN);
}
