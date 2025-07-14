//
// Created by linus on 30.06.25.
//

#ifndef MADT_H
#define MADT_H
#include "acpi.h"
#include <stdint.h>

#define MAX_CPU_CORES 64

namespace MADT {
    struct CPUCore {
        uint32_t apic_id;
        uint32_t acpi_processor_id;
        bool is_bsp;        // Bootstrap Processor
        bool is_online;
        bool is_enabled;
        void* stack_ptr;    // Stack für diesen Core
        uint64_t current_task_id;
    };
    
    void parse_madt(ACPI::MADTHeader* madt);
    uint32_t get_cpu_count();
    CPUCore* get_cpu_cores();
    uint32_t get_bsp_apic_id();
}
#endif //MADT_H
