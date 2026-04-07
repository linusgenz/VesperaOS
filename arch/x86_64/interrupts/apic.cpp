#include "vespera/log.h"
#if DEBUG_SPINLOCK
#include "../../../kernel/debug/deadlock_detector.h"
#endif

#include <vespera/kerrno.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>

#include "../../../include/vespera/cpu/io.h"
#include "../../../kernel/acpi/acpi_manager.h"
#include "../../../kernel/acpi/madt.h"
#include "../../../kernel/cpu/cpu_manager.h"
#include "apic.h"
#include "interrupts_internal.h"

namespace arch::x86_64::interrupts::apic {
    u32 read(const u32 offset) {
        volatile auto *reg = reinterpret_cast<volatile u32 *>(g_local_apic_addr + offset);
        return *reg;
    }

    void write(const u32 offset, const u32 value) {
        volatile auto *reg = reinterpret_cast<volatile u32 *>(g_local_apic_addr + offset);
        *reg = value;
    }

    void wait_for_delivery() {
        // Wait for delivery to complete
        while (read(LAPIC_ICRLO) & ICR_DELIVS) {
            asm volatile("pause");  // TODO
        }
    }

    void init(const u8 cpu_id) {
        write(LAPIC_TPR, 0);

        // Logical Destination Mode
        write(LAPIC_DFR, 0xffffffff);    // Flat mode
        write(LAPIC_LDR, cpu_id << 24);  // 0x01000000   // All cpus use logical id 1

        // Configure Spurious Interrupt Vector Register
        write(LAPIC_SVR, 0x100 | IRQ_SPURIOUS);

        write(LAPIC_TDCR, 0x3);  // Divide by 16
        write(LAPIC_TICR, 0xFFFFFFFF);

        pmt_delay(10000);  // TODO eventuell auf 1ms gehen, für mehr präzision [every 10 ms = 1 interrupt]

        const u32 calibration = 0xffffffff - read(LAPIC_TCCR);
        write(LAPIC_TIMER, IRQ_TIMER | LAPIC_PERIODIC);
        write(LAPIC_TDCR, 0x3);  // 16
        write(LAPIC_TICR, calibration);

        write(LAPIC_ICRLO, 0x0);  // zero this shit
        write(LAPIC_ICRHI, 0x0);
    }

    void send_ipi(const u32 apic_id, const u8 vector) {
        // Set destination
        write(LAPIC_ICRHI, apic_id << 24);

        const u32 icr_lo = static_cast<u32>(vector) | APIC_ICR_LEVEL_ASSERT;

        write(LAPIC_ICRLO, icr_lo);

        wait_for_delivery();
    }

    void broadcast_ipi(const u8 vector) {
        const u32 self_apic_id = local_apic_get_id();

        for (u32 i = 0; i < cpu_manager::total_cpus && i < MAX_CPU_CORES; i++) {
            const auto &cpu = cpu_manager::cpu_infos[i];

            if (cpu.apic_id == self_apic_id) continue;

            send_ipi(cpu.apic_id, vector);
        }
    }

    void pmt_delay(const usize us) {
        const acpi::FADT *fadt = acpi::TableManager::get_fadt();

        if (fadt->pm_timer_length != 4) {
            kernel::SystemManager::system_panic("ACPI Timer unavailable", -KENOACPI);
        }

        const u64 count = inl(fadt->pm_timer_block);
        const u64 target = (us * PMT_TIMER_RATE) / 1000000;
        u64 current = 0;

        while (current < target) {
            current = ((inl(fadt->pm_timer_block) - count) & 0xffffff);
        }
    }

    u32 local_apic_get_id() {
        return read(LAPIC_ID) >> 24;
    }

    void timer_accounting() {
        const u32 cpu = cpu_manager::get_current_cpu_id();
        apic_ticks[cpu]++;
    }

    void timer_tick(TrapFrame *frame) {
#if DEBUG_SPINLOCK
        deadlock_detector_tick();
#endif

        if (!kernel::scheduling::is_initialized()) return;
        const u32 cpu = cpu_manager::get_current_cpu_id();

        const Unit* current = kernel::scheduling::get_current_unit();
        const bool is_idle = !current || current->is_idle;

        cpu_manager::accounting_tick(cpu, is_idle);

        kernel::scheduling::wake_sleeping_units(cpu, apic_ticks[cpu]);
        kernel::scheduling::tick_cpu(cpu, frame);
    }

    void sleep(u64 ms) {
        u64 ticks_to_wait = (ms + 9) / 10;
        u32 cpu = cpu_manager::get_current_cpu_id();
        u64 target = apic_ticks[cpu] + ticks_to_wait;

        while (apic_ticks[cpu] < target) {
            asm volatile("hlt");
        }
    }

    void send_eoi() {
        write(LAPIC_EOI, 0);
    }
}  // namespace arch::x86_64::interrupts::apic
