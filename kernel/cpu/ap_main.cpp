//
// Created by Linus on 17.07.25.
//
#include "cpu_manager.h"
#include <log.h>
#include <kernel/system/system_manager.h>
#include <kernel/interrupts.h>
#include <kernel/kerrno.h>
#include <kernel/scheduling.h>

extern "C" void ap_main(uint32_t apic_id)
{
    const uint32_t cpu_id = CPUManager::get_current_cpu_id();

    kernel::interrupts::lapic_init(cpu_id);

    if (!cpu_id)
    {
        kernel::SystemManager::system_panic("Failed to find CPU ID", -KENOCPUID);
    }

    Log::Ok("Cpu %u initialized", cpu_id);

    kernel::scheduling::enable_on_cpu(cpu_id);

    kernel::SystemManager::system_panic("AP core returned from context switch", -KEAPRETURN);
}
