#include <acpi/acpi_subsystem.h>
#include <klib/string.h>

#include "idt.h"
#include "vespera/log.h"
#include "vespera/time.h"
#if DEBUG_SPINLOCK
#include <kernel/debug/deadlock_detector.h>
#endif

#include <acpi/madt.h>
#include <vespera/cpu/cpu_manager.h>
#include <vespera/cpu/io.h>
#include <vespera/kerrno.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>

#include "apic.h"
#include "interrupts_internal.h"

namespace arch::x86_64::interrupts::apic {

    static u32 g_apic_cal[kernel::acpi::madt::MAX_CPU_CORES] = {};

    static u64 apic_ticks[kernel::acpi::madt::MAX_CPU_CORES] = {};

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

    static u32 ns_to_counts(const u8 cpu_id, const u64 ns) {
        const u32 cal = g_apic_cal[cpu_id];
        if (cal == 0) return 1;

        // Use 128-bit-wide intermediate to avoid overflow at high TSC rates.
        // cal_window_ns = APIC_CAL_WINDOW_US * 1000
        constexpr u64 CAL_WINDOW_NS = static_cast<u64>(APIC_CAL_WINDOW_US) * 1'000ULL;

        const u64 counts = (ns * static_cast<u64>(cal)) / CAL_WINDOW_NS;
        if (counts == 0) return 1;
        if (counts > 0xFFFF'FFFFull) return 0xFFFF'FFFFu;
        return static_cast<u32>(counts);
    }

    void arm_oneshot_ns(u64 ns) {
        if (ns < APIC_MIN_DELAY_NS) ns = APIC_MIN_DELAY_NS;

        const u8 cpu_id = static_cast<u8>(cpu_manager::get_current_cpu_id());
        const u32 counts = ns_to_counts(cpu_id, ns);

        // One-shot mode: LAPIC_TIMER_ONESHOT (= 0) – no periodic bit.
        write(LAPIC_TIMER, IRQ_TIMER | LAPIC_TIMER_ONESHOT);
        write(LAPIC_TDCR, 0x3);  // divide by 16 (same as calibration)
        write(LAPIC_TICR, counts);
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

        pmt_delay(APIC_CAL_WINDOW_US);  // TODO eventuell auf 1ms gehen, für mehr präzision [every 10 ms = 1 interrupt]

        const u32 calibration = 0xFFFF'FFFFu - read(LAPIC_TCCR);
        g_apic_cal[cpu_id] = calibration;

        write(LAPIC_ICRLO, 0x0);  // zero this shit
        write(LAPIC_ICRHI, 0x0);

        kernel::time::sleep_timer::start(cpu_id);
    }

    void init_bsp() {
        memset(apic_ticks, 0, kernel::acpi::madt::MAX_CPU_CORES * sizeof(u64));

        init(0);
    }

    void send_ipi(const u32 apic_id, const u8 vector) {
        // Set destination
        write(LAPIC_ICRHI, apic_id << 24);

        const u32 icr_lo = static_cast<u32>(vector) | APIC_ICR_LEVEL_ASSERT;

        write(LAPIC_ICRLO, icr_lo);

        wait_for_delivery();
    }

    u32 get_id() {
        return read(LAPIC_ID) >> 24;
    }

    void broadcast_ipi(const u8 vector) {
        const u32 self_apic_id = get_id();

        cpu_manager::for_each_online_cpu([&](const cpu_manager::CpuInfo &cpu) {
            if (cpu.apic_id != self_apic_id) {
                send_ipi(cpu.apic_id, vector);
            }
        });
    }

    void halt_cpus() {
        broadcast_ipi(IRQ_PANIC);
    }

    void pmt_delay(const usize us) {
        const kernel::acpi::FADT *fadt = kernel::acpi::get_fadt();

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

    void timer_accounting() {
        const u32 cpu = cpu_manager::get_current_cpu_id();
        apic_ticks[cpu]++;
    }

    void timer_tick(TrapFrame *frame) {
#if DEBUG_SPINLOCK
        deadlock_detector_tick();
#endif
        if (!kernel::scheduling::is_initialized()) {
            // Scheduler not ready yet – just re-arm and return.
            arm_oneshot_ns(QUANTUM_NS);
            return;
        }

        const u32 cpu = cpu_manager::get_current_cpu_id();

        cpu_manager::accounting_tick(cpu, kernel::scheduling::is_current_unit_idle());

        // 1. Wake any units whose deadline has passed.
        kernel::scheduling::wake_sleeping_units(cpu);

        // 2. Run the scheduler quantum tick (may call yield_cpu internally).
        kernel::scheduling::tick_cpu(cpu, frame);

        // 3. Re-arm the APIC for the next event (quantum OR earliest sleep).
        //    This is the core of the one-shot tickless design.
        kernel::time::sleep_timer::arm_next_event(static_cast<u8>(cpu));
    }

    void sleep(u64 ms) {
        u64 ticks_to_wait = (ms + 9) / 10;
        u32 cpu = cpu_manager::get_current_cpu_id();
        u64 target = apic_ticks[cpu] + ticks_to_wait;

        while (apic_ticks[cpu] < target) {
            asm volatile("hlt");
        }
    }

    void send_init_ipi(const u32 apic_id) {
        write(LAPIC_ICRHI, apic_id << 24);
        write(LAPIC_ICRLO, APIC_ICR_INIT | APIC_ICR_LEVEL_ASSERT);
        wait_for_delivery();

        kernel::time::sleep_ms(10);

        write(LAPIC_ICRHI, apic_id << 24);
        write(LAPIC_ICRLO, APIC_ICR_INIT | ICR_DEASSERT);
        wait_for_delivery();

        kernel::time::sleep_ms(10);
    }

    void send_sipi(const u32 apic_id, const u8 vector) {
        write(LAPIC_ICRHI, apic_id << 24);
        write(LAPIC_ICRLO, vector | APIC_ICR_SIPI);
        kernel::time::sleep_ms(10);
    }

    void send_eoi() {
        write(LAPIC_EOI, 0);
    }
}  // namespace arch::x86_64::interrupts::apic
