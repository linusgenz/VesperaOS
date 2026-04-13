#include "cpu_manager.h"

#include <klib/string.h>
#include <vespera/interrupts.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/time.h>

#include "../../arch/x86_64/interrupts/apic.h"
#include "../../include/acpi/madt.h"
#include "cpu.h"

namespace cpu_manager {
    CpuInfo cpu_infos[kernel::acpi::madt::MAX_CPU_CORES];
    u8 total_cpus;
    static u8 online_cpus;
    static bool is_initialized = false;
    static u32 bsp_apic_id;

    void initialize() {
        if (is_initialized) return;

        const kernel::acpi::madt::cpu_core* madt_cores = kernel::acpi::madt::cpu_cores();
        const u32 madt_cpu_count = kernel::acpi::madt::cpu_count();
        bsp_apic_id = kernel::acpi::madt::bsp_apic_id();

        if (madt_cpu_count == 0) {
            Log::error("No CPUs found in MADT");
            return;
        }

        // init location for cpu startup reports
        for (u32 i = 0; i < total_cpus; ++i) {
            CPU_STARTUP_REPORTS[i].apic_id = 0;
            CPU_STARTUP_REPORTS[i].stack_pointer = 0;
            CPU_STARTUP_REPORTS[i].ready = false;
        }

        // Initialisiere CPU-Infos
        for (u32 i = 0; i < madt_cpu_count && i < kernel::acpi::madt::MAX_CPU_CORES; i++) {
            cpu_infos[i].apic_id = madt_cores[i].apic_id;
            cpu_infos[i].cpu_id = i;
            cpu_infos[i].state = CPU_STATE_OFFLINE;
            cpu_infos[i].kernel_stack = 0;
            cpu_infos[i].accounting.last_tick_tsc = rdtsc();
            cpu_infos[i].accounting.total_cycles = 0;
            cpu_infos[i].accounting.idle_cycles = 0;
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

    for (u32 i = 0; i < total_cpus; ++i) {
        if (cpu_infos[i].is_bsp) continue;

        u32 apic_id = cpu_infos[i].apic_id;

        virt_addr_t stack_virt = kernel::memory::request_pages(KERNEL_STACK_SIZE / PAGE_SIZE);
        auto stack_top_virt = reinterpret_cast<void*>(virt_raw(stack_virt) + KERNEL_STACK_SIZE);

        memset(stack_virt.ptr, 0, KERNEL_STACK_SIZE);

        volatile auto* report = &CPU_STARTUP_REPORTS[apic_id];
        report->apic_id      = apic_id;
        report->stack_pointer = reinterpret_cast<u64>(stack_top_virt);
        report->ready        = false;
        report->go           = false;

        cpu_infos[i].kernel_stack     = virt_raw(stack_virt);
        cpu_infos[i].kernel_stack_top = reinterpret_cast<u64>(stack_top_virt);

        asm volatile("mfence" ::: "memory");
    }

    for (u32 i = 0; i < total_cpus; ++i) {
        if (cpu_infos[i].is_bsp) continue;
        init_core(&cpu_infos[i]);
    }

    for (int i = 0; i < total_cpus; ++i) {
        if (cpu_infos[i].is_bsp) continue;

        u32 apic_id = cpu_infos[i].apic_id;
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
    constexpr u32 vector_value = 0x8;

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

    CpuInfo* get_cpu_info(const u32 apic_id) {
        if (!is_initialized) return nullptr;

        for (u32 i = 0; i < total_cpus; i++) {
            if (cpu_infos[i].apic_id == apic_id) {
                return &cpu_infos[i];
            }
        }
        return nullptr;
    }

    u8 get_current_cpu_id() {
        if (!is_initialized) return 0;

        const u32 current_apic_id = kernel::interrupts::lapic_get_id();

        for (u8 i = 0; i < total_cpus; i++) {
            if (cpu_infos[i].apic_id == current_apic_id) {
                return cpu_infos[i].cpu_id;
            }
        }

        return 0;  // Default BSP
    }

    u8 get_online_cpu_count() {
        return online_cpus;
    }

    u8 get_available_cpu_count() {
        return total_cpus;
    }

    void halt_cpu(const u32 apic_id) {
        if (CpuInfo* cpu_info = get_cpu_info(apic_id)) {
            cpu_info->state = CPU_STATE_HALTED;
        }
    }

    void send_ipi_to_all_aps(const u32 vector) {
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

        for (u32 i = 0; i < total_cpus; i++) {
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

    void accounting_tick(const u32 cpu_id, const bool is_idle) {
        if (cpu_id >= total_cpus) return;
        auto& [last_tick_tsc, total_cycles, idle_cycles] = cpu_infos[cpu_id].accounting;

        const u64 now = rdtsc();
        const u64 delta = now - last_tick_tsc;
        last_tick_tsc = now;

        total_cycles += delta;
        if (is_idle) idle_cycles += delta;
    }

    int get_cpu_usage_percent(const u32 cpu_id) {
        if (cpu_id >= total_cpus) return 0;
        const CpuAccounting& acc = cpu_infos[cpu_id].accounting;
        if (acc.total_cycles == 0) return 0;

        const u64 active = acc.total_cycles - acc.idle_cycles;
        return static_cast<int>((active * 100) / acc.total_cycles);
    }
}  // namespace CPUManager
