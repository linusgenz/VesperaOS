//
// Created by Linus on 17.07.25.
//
#include "cpu_manager.h"
#include "stdint.h"
#include "../scheduling/scheduler.h"
#include "../utils/panic.h"
#include "../../include/log.h"

extern "C" void ap_main(uint32_t apic_id) {

    uint32_t cpu_id = -1;
    for (uint32_t i = 0; i < CPUManager::total_cpus; ++i) {
        if (CPUManager::cpu_infos[i].apic_id == apic_id) {
            cpu_id = CPUManager::cpu_infos[i].cpu_id;
            break;
        }
    }

    if (!cpu_id) {
        panic("Failed to find CPU ID");
    }

    scheduler_init_cpu(cpu_id);

    panic("AP core returned from context switch");
}
