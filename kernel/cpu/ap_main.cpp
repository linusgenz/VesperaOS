//
// Created by Linus on 17.07.25.
//
#include "cpu_manager.h"
#include <cstdint>
#include <scheduling.h>
#include "../utils/panic.h"
#include "../../arch/x86_64/interrupts/apic.h"
#include "../../include/log.h"
#include "../include/interrupts.h"
#include "../system/system_manager.h"
#include <kerrno.h>

extern "C" void ap_main(uint32_t apic_id) {
    const uint32_t cpu_id = CPUManager::get_current_cpu_id();

    kernel::interrupts::lapic_init(cpu_id);

    if (!cpu_id) {
        kernel::SystemManager::system_panic("Failed to find CPU ID", -ENOCPUID);
    }

    Log::Ok("Cpu %u initialized", cpu_id);

    kernel::scheduling::enable_on_cpu(cpu_id);

    kernel::SystemManager::system_panic("AP core returned from context switch", -EAPRETURN);
}
