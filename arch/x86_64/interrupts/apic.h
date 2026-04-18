//
// Created by linus on 30.06.25.
//

#ifndef APIC_H
#define APIC_H
#include <acpi/madt.h>
#include <vespera/types.h>

#include "interrupts_internal.h"

inline volatile u8* g_local_apic_addr;

namespace arch::x86_64::interrupts::apic {
    constexpr u64 APIC_TICK_HZ = 100;

    // Calibration window used during init() – the longer, the more accurate.
    constexpr usize APIC_CAL_WINDOW_US = 10'000;  // 10 ms

    // Default scheduler quantum expressed as a nanosecond budget.
    // sleep_timer uses this when there are no sleeping units pending.
    constexpr u64 APIC_QUANTUM_NS = 10'000'000ULL;  // 10 ms

    // Minimum one-shot delay to avoid re-arming with 0 counts.
    constexpr u64 APIC_MIN_DELAY_NS = 50'000ULL;  // 50 µs

    // ------------------------------------------------------------------------------------------------
    // Local APIC Registers
#define LAPIC_ID 0x0020       // Local APIC ID
#define LAPIC_VER 0x0030      // Local APIC Version
#define LAPIC_TPR 0x0080      // Task Priority
#define LAPIC_APR 0x0090      // Arbitration Priority
#define LAPIC_PPR 0x00a0      // Processor Priority
#define LAPIC_EOI 0x00b0      // EOI
#define LAPIC_RRD 0x00c0      // Remote Read
#define LAPIC_LDR 0x00d0      // Logical Destination
#define LAPIC_DFR 0x00e0      // Destination Format
#define LAPIC_SVR 0x00f0      // Spurious Interrupt Vector
#define LAPIC_ISR 0x0100      // In-Service (8 registers)
#define LAPIC_TMR 0x0180      // Trigger Mode (8 registers)
#define LAPIC_IRR 0x0200      // Interrupt Request (8 registers)
#define LAPIC_ESR 0x0280      // Error Status
#define LAPIC_ICRLO 0x0300    // Interrupt Command
#define LAPIC_ICRHI 0x0310    // Interrupt Command [63:32]
#define LAPIC_TIMER 0x0320    // LVT Timer
#define LAPIC_THERMAL 0x0330  // LVT Thermal Sensor
#define LAPIC_PERF 0x0340     // LVT Performance Counter
#define LAPIC_LINT0 0x0350    // LVT LINT0
#define LAPIC_LINT1 0x0360    // LVT LINT1
#define LAPIC_ERROR 0x0370    // LVT Error
#define LAPIC_TICR 0x0380     // Initial Count (for Timer)
#define LAPIC_TCCR 0x0390     // Current Count (for Timer)
#define LAPIC_TDCR 0x03e0     // Divide Configuration (for Timer)

    // ICR bits
#define ICR_DELIVS 0x00001000    // Delivery status
#define ICR_DEASSERT 0x00000000  // Deassert
#define ICR_LEVEL 0x00008000     // Level triggered
#define APIC_REGISTER_INT_COMMAND_LOW 0x300
#define APIC_REGISTER_INT_COMMAND_HIGH 0x310
#define APIC_ICR_INIT (5 << 8)
#define APIC_ICR_SIPI (6 << 8)
#define APIC_ICR_LEVEL_ASSERT (1 << 14)

#define LAPIC_TIMER_ONESHOT 0x00000000
#define LAPIC_PERIODIC 0x20000
#define PMT_TIMER_RATE 3579545  // 3.57 MHz

    void send_eoi();
    void timer_accounting();
    void timer_tick(TrapFrame* frame);
    void init(u8 cpu_id);
    void arm_oneshot_ns(u64 ns);
    void send_ipi(u32 apic_id, u8 vector);
    void broadcast_ipi(u8 vector);
    void wait_for_delivery();

    inline u32 g_apic_cal[kernel::acpi::madt::MAX_CPU_CORES] = {};

    inline u64 apic_ticks[kernel::acpi::madt::MAX_CPU_CORES] = {};

    u32 local_apic_get_id();
    void write(u32 offset, u32 value);
    u32 read(u32 offset);

    void pmt_delay(usize us);
}  // namespace arch::x86_64::interrupts::apic

#endif  // APIC_H
