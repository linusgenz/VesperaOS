#include "cpu_manager.h"

#include <kernel/interrupts.h>
#include <kernel/time.h>

#include "../../arch/x86_64/interrupts/apic.h"
#include "../../include/log.h"
#include "../acpi/madt.h"
#include "kernel/memory.h"

namespace cpu_manager {
    // Globale Variablen
    CpuInfo cpu_infos[MAX_CPU_CORES];
    uint8_t total_cpus;
    static uint8_t online_cpus;
    static bool is_initialized = false;
    static uint32_t bsp_apic_id;

    void initialize() {
        if (is_initialized) return;

        // Hole CPU-Informationen von MADT
        madt::CpuCore* madt_cores = madt::get_cpu_cores();
        uint32_t madt_cpu_count = madt::get_cpu_count();
        bsp_apic_id = madt::get_bsp_apic_id();

        if (madt_cpu_count == 0) {
            Log::error("No CPUs found in MADT");
            return;
        }

        // init location for cpu startup reports
        for (uint32_t i = 0; i < total_cpus; ++i) {
            CPU_STARTUP_REPORTS[i].apic_id = 0;
            CPU_STARTUP_REPORTS[i].stack_pointer = 0;
            CPU_STARTUP_REPORTS[i].ready = false;
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

            // BSP ist bereits online
            if (cpu_infos[i].is_bsp) {
                cpu_infos[i].state = CPU_STATE_ONLINE;
                cpu_infos[i].kernel_stack = KERNEL_STACK_BASE;
                cpu_infos[i].kernel_stack_top = KERNEL_STACK_BASE + KERNEL_STACK_SIZE;
                online_cpus++;

                // TODO set bsp stack so it does not use limine stack
            /*    kernel::memory::map_memory(
                    reinterpret_cast<void*>(KERNEL_STACK_BASE), reinterpret_cast<void*>(KERNEL_STACK_BASE)
                );*/
            }

            total_cpus++;
        }

        is_initialized = true;
        Log::ok("CPU Manager initialized - %u CPUs detected, %u online", total_cpus, online_cpus);
    }

void smp_init() {

    for (uint32_t i = 0; i < total_cpus; ++i) {
        if (cpu_infos[i].is_bsp) continue;

        uint32_t apic_id = cpu_infos[i].apic_id;

        virt_addr_t stack_virt = kernel::memory::request_pages(KERNEL_STACK_SIZE / PAGE_SIZE);
        auto stack_top_virt = reinterpret_cast<void*>(virt_raw(stack_virt) + KERNEL_STACK_SIZE);

        memset(stack_virt.ptr, 0, KERNEL_STACK_SIZE);

        volatile auto* report = &CPU_STARTUP_REPORTS[apic_id];
        report->apic_id      = apic_id;
        report->stack_pointer = reinterpret_cast<uint64_t>(stack_top_virt);
        report->ready        = false;
        report->go           = false;

        cpu_infos[i].kernel_stack     = virt_raw(stack_virt);
        cpu_infos[i].kernel_stack_top = reinterpret_cast<uint64_t>(stack_top_virt);

        asm volatile("mfence" ::: "memory");
    }

    for (uint32_t i = 0; i < total_cpus; ++i) {
        if (cpu_infos[i].is_bsp) continue;
        init_core(&cpu_infos[i]);
    }

    for (int i = 0; i < total_cpus; ++i) {
        if (cpu_infos[i].is_bsp) continue;

        uint32_t apic_id = cpu_infos[i].apic_id;
        volatile auto* report = &CPU_STARTUP_REPORTS[apic_id];

        int timeout = 0;
        while (!report->ready && timeout++ < 100)
            kernel::time::sleep_ms(10);

        if (timeout >= 100) {
            Log::warning("CPU %u (APIC %u) timed out", i, apic_id);
            continue;
        }

        online_cpus++;
        cpu_infos[i].state = CPU_STATE_ONLINE;
        report->go = true;

        asm volatile("mfence" ::: "memory");
    }
}

    void init_core(const CpuInfo* cpu) {
    constexpr uint32_t vector_value = 0x8;

        // Send INIT
        kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_HIGH, cpu->apic_id << 24);
        kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_LOW, APIC_ICR_INIT | APIC_ICR_LEVEL_ASSERT);
        kernel::interrupts::lapic_wait_for_delivery();

        kernel::time::internal::sleep(10);

        kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_HIGH, cpu->apic_id << 24);
        kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_LOW, APIC_ICR_INIT | ICR_DEASSERT);
        kernel::interrupts::lapic_wait_for_delivery();

        kernel::time::internal::sleep(10);
        kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_HIGH, cpu->apic_id << 24);
        kernel::interrupts::lapic_write(APIC_REGISTER_INT_COMMAND_LOW, vector_value | APIC_ICR_SIPI);

        kernel::time::internal::sleep(10);
    }

    CpuInfo* get_cpu_info(const uint32_t apic_id) {
        if (!is_initialized) return nullptr;

        for (uint32_t i = 0; i < total_cpus; i++) {
            if (cpu_infos[i].apic_id == apic_id) {
                return &cpu_infos[i];
            }
        }
        return nullptr;
    }

    uint8_t get_current_cpu_id() {
        if (!is_initialized) return 0;

        const uint32_t current_apic_id = kernel::interrupts::lapic_get_id();

        for (uint8_t i = 0; i < total_cpus; i++) {
            if (cpu_infos[i].apic_id == current_apic_id) {
                return cpu_infos[i].cpu_id;
            }
        }

        return 0;  // Default BSP
    }

    uint8_t get_online_cpu_count() {
        return online_cpus;
    }

    uint8_t get_available_cpu_count() {
        return total_cpus;
    }

    void halt_cpu(const uint32_t apic_id) {
        if (CpuInfo* cpu_info = get_cpu_info(apic_id)) {
            cpu_info->state = CPU_STATE_HALTED;
        }
    }

    void send_ipi_to_all_aps(const uint32_t vector) {
        // Broadcast IPI an alle APs (außer BSP)
        kernel::interrupts::lapic_write(LAPIC_ICRHI, 0);
        kernel::interrupts::lapic_write(LAPIC_ICRLO, 0xC0000 | vector);  // All except self, vector
    }

    // this shit is disabling interrupts idk how but it does
    void print_cpu_info() {
        if (!is_initialized) {
            Log::info("CPU Manager not initialized");
            return;
        }

        Log::info("=== CPU Manager Info ===");
        Log::info("Total CPUs: %u, Online: %u", total_cpus, online_cpus);
        Log::info("BSP APIC ID: %u", bsp_apic_id);

        for (uint32_t i = 0; i < total_cpus; i++) {
            const char* state_str = nullptr;
            switch (cpu_infos[i].state) {
                case CPU_STATE_OFFLINE:
                    state_str = "OFFLINE";
                    break;
                case CPU_STATE_STARTING:
                    state_str = "STARTING";
                    break;
                case CPU_STATE_ONLINE:
                    state_str = "ONLINE";
                    break;
                case CPU_STATE_HALTED:
                    state_str = "HALTED";
                    break;
                default:
                    state_str = "UNKNOWN";
                    break;
            }

            Log::info(
                "CPU %u: APIC ID %u, %s, %s, Stack: %p",
                cpu_infos[i].cpu_id,
                cpu_infos[i].apic_id,
                cpu_infos[i].is_bsp ? "BSP" : "AP",
                state_str,
                cpu_infos[i].kernel_stack
            );
        }
    }

    void update_cpu_stats(const uint32_t apic_id, const uint64_t cycles, const uint64_t idle_cycles) {
        if (CpuInfo* cpu_info = get_cpu_info(apic_id)) {
            cpu_info->total_cycles = cycles;
            cpu_info->idle_cycles = idle_cycles;
        }
    }

    int get_cpu_usage(const uint32_t apic_id) {
        CpuInfo* cpu_info = get_cpu_info(apic_id);
        if (!cpu_info || cpu_info->total_cycles == 0) {
            return 0.0;
        }

        uint64_t active_cycles = cpu_info->total_cycles - cpu_info->idle_cycles;
        return ((active_cycles) / cpu_info->total_cycles) * 100;
    }
}  // namespace CPUManager
