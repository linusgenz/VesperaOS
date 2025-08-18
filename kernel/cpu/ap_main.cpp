//
// Created by Linus on 17.07.25.
//
#include "cpu_manager.h"
#include "stdint.h"
#include <scheduling.h>
#include "../utils/panic.h"
#include "../../arch/x86_64/interrupts/apic.h"
#include "../../include/log.h"
#include "../include/interrupts.h"

extern "C" void ap_main(uint32_t apic_id) {

    const uint32_t cpu_id = CPUManager::get_current_cpu_id();

    kernel::interrupts::lapic_init(cpu_id);

    if (!cpu_id) {
        panic("Failed to find CPU ID");
    }

    kernel::scheduling::enable_on_cpu(cpu_id);
    kernel::scheduling::yield();

    panic("AP core returned from context switch");
}
