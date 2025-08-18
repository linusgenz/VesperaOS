#include "cpu_manager.h"
#include "../../arch/x86_64/interrupts/apic.h"
#include "../include/memory.h"
#include "../../include/log.h"
#include "../acpi/madt.h"
#include "../include/interrupts.h"
#include "../time/time.h"
#include <scheduling.h>

namespace CPUManager {
    // Globale Variablen
    CPUInfo cpu_infos[MAX_CPU_CORES];
    uint32_t total_cpus;
    static uint32_t online_cpus;
    static bool is_initialized = false;
    static uint32_t bsp_apic_id;

    void initialize() {
        if (is_initialized) return;

        // Initialisiere Stack Manager
        StackManager::initialize();

        // Hole CPU-Informationen von MADT
        MADT::CPUCore *madt_cores = MADT::get_cpu_cores();
        uint32_t madt_cpu_count = MADT::get_cpu_count();
        bsp_apic_id = MADT::get_bsp_apic_id();

        if (madt_cpu_count == 0) {
            Log::Error("No CPUs found in MADT");
            return;
        }

        // Initialisiere CPU-Infos
        for (uint32_t i = 0; i < madt_cpu_count && i < MAX_CPU_CORES; i++) {
            cpu_infos[i].apic_id = madt_cores[i].apic_id;
            cpu_infos[i].cpu_id = i;
            cpu_infos[i].state = CPU_STATE_OFFLINE;
            cpu_infos[i].kernel_stack = 0;
            cpu_infos[i].total_cycles = 0;
            cpu_infos[i].idle_cycles = 0;
            cpu_infos[i].current_task_id = 0;
            cpu_infos[i].is_bsp = madt_cores[i].is_bsp;

            // Allokiere Kernel-Stack für jeden CPU
            //    cpu_infos[i].kernel_stack = StackManager::allocate_kernel_stack(i);
            //  if (!cpu_infos[i].kernel_stack) {
            //    Log::Error("Failed to allocate kernel stack for CPU %u", i);
            //  continue;
            //}

            // BSP ist bereits online
            if (cpu_infos[i].is_bsp) {
                cpu_infos[i].state = CPU_STATE_ONLINE;
                online_cpus++;
            }

            total_cpus++;
        }

        is_initialized = true;
        Log::Ok("CPU Manager initialized - %u CPUs detected, %u online", total_cpus, online_cpus);
    }

    void smp_init() {
        // Send Startup to all cpus except self

        for (uint32_t i = 0; i < total_cpus; ++i) {
            if (cpu_infos[i].is_bsp) continue;
            init_core(&cpu_infos[i]);
        }


        for (int i = 0; i < total_cpus; ++i) {
            if (cpu_infos[i].is_bsp) continue;
            uint32_t apic_id = cpu_infos[i].apic_id;
            volatile CpuStartupReport *report = &cpu_startup_reports[apic_id];

            int timeout_counter = 0;
            const int timeout_limit = 20;
            while (!report->ready && timeout_counter < timeout_limit) {
                kernel::time::sleep_ms(100);
                timeout_counter++;
            }
            if (timeout_counter == 20) continue;
            online_cpus++;
            cpu_infos[i].kernel_stack = report->stack_pointer;
            cpu_infos[i].state = CPU_STATE_ONLINE;
        }
    }


    void init_core(CPUInfo *cpu) {
        uint32_t vectorValue = 0x8;

        // Send INIT
        kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_HIGH, cpu->apic_id << 24);
        kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_LOW, APIC_ICR_INIT | APIC_ICR_LEVEL_ASSERT);
        kernel::interrupts::lapic_wait_for_delivery();

        kernel::time::sleep_ms(10);

        kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_HIGH, cpu->apic_id << 24);
        kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_LOW,
                                        APIC_ICR_INIT | ICR_DEASSERT);
        kernel::interrupts::lapic_wait_for_delivery();

        kernel::time::sleep_ms(10);
        kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_HIGH, cpu->apic_id << 24);
        kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_LOW, vectorValue | APIC_ICR_SIPI);

        kernel::time::sleep_ms(10);

        // kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_HIGH, cpu->apic_id << 24);
        //  kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_LOW, vectorValue | APIC_ICR_SIPI);

        kernel::time::sleep_ms(10);
    }

    CPUInfo *get_cpu_info(uint32_t apic_id) {
        if (!is_initialized) return nullptr;

        for (uint32_t i = 0; i < total_cpus; i++) {
            if (cpu_infos[i].apic_id == apic_id) {
                return &cpu_infos[i];
            }
        }
        return nullptr;
    }

    uint32_t get_current_cpu_id() {
        if (!is_initialized) return 0;

        uint32_t current_apic_id = kernel::interrupts::lapic_get_id();

        for (uint32_t i = 0; i < total_cpus; i++) {
            if (cpu_infos[i].apic_id == current_apic_id) {
                return cpu_infos[i].cpu_id;
            }
        }

        return 0; // Default BSP
    }

    uint32_t get_online_cpu_count() {
        return online_cpus;
    }

    uint32_t get_available_cpu_count() {
        return total_cpus;
    }

    void halt_cpu(uint32_t apic_id) {
        CPUInfo *cpu_info = get_cpu_info(apic_id);
        if (cpu_info) {
            cpu_info->state = CPU_STATE_HALTED;
        }
    }


    void send_ipi_to_all_aps(uint32_t vector) {
        // Broadcast IPI an alle APs (außer BSP)
        kernel::interrupts::lapic_write(LAPIC_ICRHI, 0);
        kernel::interrupts::lapic_write(LAPIC_ICRLO, 0xC0000 | vector); // All except self, vector
    }

    // this shit is disabling interrupts idk how but it does
    void print_cpu_info() {
        if (!is_initialized) {
            Log::Info("CPU Manager not initialized");
            return;
        }

        Log::Info("=== CPU Manager Info ===");
        Log::Info("Total CPUs: %u, Online: %u", total_cpus, online_cpus);
        Log::Info("BSP APIC ID: %u", bsp_apic_id);

        for (uint32_t i = 0; i < total_cpus; i++) {
            const char *state_str;
            switch (cpu_infos[i].state) {
                case CPU_STATE_OFFLINE: state_str = "OFFLINE";
                    break;
                case CPU_STATE_STARTING: state_str = "STARTING";
                    break;
                case CPU_STATE_ONLINE: state_str = "ONLINE";
                    break;
                case CPU_STATE_HALTED: state_str = "HALTED";
                    break;
                default: state_str = "UNKNOWN";
                    break;
            }

            Log::Info("CPU %u: APIC ID %u, %s, %s, Stack: %p",
                      cpu_infos[i].cpu_id,
                      cpu_infos[i].apic_id,
                      cpu_infos[i].is_bsp ? "BSP" : "AP",
                      state_str,
                      cpu_infos[i].kernel_stack);
        }
    }

    void update_cpu_stats(uint32_t apic_id, uint64_t cycles, uint64_t idle_cycles) {
        CPUInfo *cpu_info = get_cpu_info(apic_id);
        if (cpu_info) {
            cpu_info->total_cycles = cycles;
            cpu_info->idle_cycles = idle_cycles;
        }
    }

    double get_cpu_usage(uint32_t apic_id) {
        CPUInfo *cpu_info = get_cpu_info(apic_id);
        if (!cpu_info || cpu_info->total_cycles == 0) {
            return 0.0;
        }

        uint64_t active_cycles = cpu_info->total_cycles - cpu_info->idle_cycles;
        return ((double) active_cycles / (double) cpu_info->total_cycles) * 100.0;
    }
}
