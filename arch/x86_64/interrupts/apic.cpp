#if DEBUG_SPINLOCK
#include "../../../kernel/debug/deadlock_detector.h"
#endif

#include "apic.h"
#include <kernel/kerrno.h>
#include "../../../include/log.h"
#include "../../../include/string.h"
#include "../../../kernel/acpi/acpi_manager.h"
#include "../../../kernel/cpu/io.h"
#include "../../../kernel/acpi/madt.h"
#include "../../../kernel/cpu/cpu_manager.h"
#include <kernel/scheduling.h>
#include <kernel/system/system_manager.h>
#include "interrupts_internal.h"

namespace arch::x86_64::interrupts::apic {
    uint32_t read(const uint32_t offset) {
        volatile auto *reg = reinterpret_cast<volatile uint32_t *>(g_localApicAddr + offset);
        return *reg;
    }

    void write(const uint32_t offset, const uint32_t value) {
        volatile auto *reg = reinterpret_cast<volatile uint32_t *>(g_localApicAddr + offset);
        *reg = value;
    }

    void wait_for_delivery() {
        // Wait for delivery to complete
        while (read(LAPIC_ICRLO) & ICR_DELIVS) {
            asm volatile("pause"); // TODO
        }
    }

    void init(const uint8_t cpu_id) {
        write(LAPIC_TPR, 0);

        // Logical Destination Mode
        write(LAPIC_DFR, 0xffffffff); // Flat mode
        write(LAPIC_LDR, cpu_id << 24); // 0x01000000   // All cpus use logical id 1

        // Configure Spurious Interrupt Vector Register
        write(LAPIC_SVR, 0x100 | IRQ_SPURIOUS);

        write(LAPIC_TDCR, 0x3); // Divide by 16
        write(LAPIC_TICR, 0xFFFFFFFF);

        pmt_delay(10000); // TODO eventuell auf 1ms gehen, für mehr präzision [every 10 ms = 1 interrupt]

        uint32_t calibration = 0xffffffff - read(LAPIC_TCCR);
        write(LAPIC_TIMER, IRQ_TIMER | LAPIC_PERIODIC);
        write(LAPIC_TDCR, 0x3); // 16
        write(LAPIC_TICR, calibration);

        write(LAPIC_ICRLO, 0x0); // zero this shit
        write(LAPIC_ICRHI, 0x0);
    }

    void send_ipi(const uint32_t apic_id, const uint8_t vector) {
        // Set destination
        write(LAPIC_ICRHI, apic_id << 24);

        const uint32_t icr_lo = static_cast<uint32_t>(vector) | APIC_ICR_LEVEL_ASSERT;

        write(LAPIC_ICRLO, icr_lo);

        wait_for_delivery();
    }

    void broadcast_ipi(const uint8_t vector) {
        uint32_t self_apic_id = local_apic_get_id();

        for (uint32_t i = 0; i < CPUManager::total_cpus && i < MAX_CPU_CORES; i++) {

            const auto& cpu = CPUManager::cpu_infos[i];

            if (cpu.apic_id == self_apic_id)
                continue;

            send_ipi(cpu.apic_id, vector);
        }
    }



    void pmt_delay(const size_t us) {
        ACPI::FADT *fadt = ACPI::TableManager::get_fadt();

        if (fadt->pm_timer_length != 4) {
            kernel::SystemManager::system_panic("ACPI Timer unavailable", -KENOACPI);
        }

        const uint64_t count = inl(fadt->pm_timer_block);
        const uint64_t target = (us * PMT_TIMER_RATE) / 1000000;
        uint64_t current = 0;

        while (current < target) {
            current = ((inl(fadt->pm_timer_block) - count) & 0xffffff);
        }
    }

    uint32_t local_apic_get_id() {
        return read(LAPIC_ID) >> 24;
    }

    void timer_accounting() {
        uint32_t cpu = CPUManager::get_current_cpu_id();
        apic_ticks[cpu]++;
    }

    void timer_tick(trap_frame *frame) {

#if DEBUG_SPINLOCK
        deadlock_detector_tick();
#endif

        if (!kernel::scheduling::is_initialized()) return;
        uint32_t cpu = CPUManager::get_current_cpu_id();

        kernel::scheduling::wake_sleeping_units(cpu, apic_ticks[cpu]);

        kernel::scheduling::tick_cpu(cpu, frame);
    }


    void sleep(uint64_t ms) {
        uint64_t ticks_to_wait = (ms + 9) / 10;
        uint32_t cpu = CPUManager::get_current_cpu_id();
        uint64_t target = apic_ticks[cpu] + ticks_to_wait;

        while (apic_ticks[cpu] < target) {
            asm volatile("hlt");
        }
    }

    void send_eoi() {
        write(LAPIC_EOI, 0);
    }
}
