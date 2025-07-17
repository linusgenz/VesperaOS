//
// Created by linus on 30.06.25.
//

#ifndef APIC_H
#define APIC_H
#include <stdint.h>
#include <stddef.h>

// ------------------------------------------------------------------------------------------------
// Local APIC Registers
#define LAPIC_ID                        0x0020  // Local APIC ID
#define LAPIC_VER                       0x0030  // Local APIC Version
#define LAPIC_TPR                       0x0080  // Task Priority
#define LAPIC_APR                       0x0090  // Arbitration Priority
#define LAPIC_PPR                       0x00a0  // Processor Priority
#define LAPIC_EOI                       0x00b0  // EOI
#define LAPIC_RRD                       0x00c0  // Remote Read
#define LAPIC_LDR                       0x00d0  // Logical Destination
#define LAPIC_DFR                       0x00e0  // Destination Format
#define LAPIC_SVR                       0x00f0  // Spurious Interrupt Vector
#define LAPIC_ISR                       0x0100  // In-Service (8 registers)
#define LAPIC_TMR                       0x0180  // Trigger Mode (8 registers)
#define LAPIC_IRR                       0x0200  // Interrupt Request (8 registers)
#define LAPIC_ESR                       0x0280  // Error Status
#define LAPIC_ICRLO                     0x0300  // Interrupt Command
#define LAPIC_ICRHI                     0x0310  // Interrupt Command [63:32]
#define LAPIC_TIMER                     0x0320  // LVT Timer
#define LAPIC_THERMAL                   0x0330  // LVT Thermal Sensor
#define LAPIC_PERF                      0x0340  // LVT Performance Counter
#define LAPIC_LINT0                     0x0350  // LVT LINT0
#define LAPIC_LINT1                     0x0360  // LVT LINT1
#define LAPIC_ERROR                     0x0370  // LVT Error
#define LAPIC_TICR                      0x0380  // Initial Count (for Timer)
#define LAPIC_TCCR                      0x0390  // Current Count (for Timer)
#define LAPIC_TDCR                      0x03e0  // Divide Configuration (for Timer)

// ICR bits

#define ICR_DELIVS     0x00001000   // Delivery status
#define ICR_ASSERT     0x00004000   // Assert interrupt (vs deassert)
#define ICR_DEASSERT   0x00000000   // Deassert
#define ICR_LEVEL      0x00008000   // Level triggered
#define ICR_BCAST      0x00080000   // Send to all APICs
#define ICR_BUSY       0x00001000   // Delivery status bit
#define ICR_FIXED      0x00000000   // Fixed delivery mode
#define ICR_NO_SHORTHAND                0x00000000
#define ICR_EDGE                        0x00000000
#define ICR_LEVEL                       0x00008000
#define APIC_REGISTER_INT_COMMAND_LOW 0x300
#define APIC_REGISTER_INT_COMMAND_HIGH 0x310
#define APIC_ICR_SMI (2 << 8)
#define APIC_ICR_INIT (5 << 8)
#define APIC_ICR_SIPI (6 << 8)
#define APIC_ICR_LEVEL_ASSERT (1 << 14)

#define IRQ_SPURIOUS         0xFF
#define IRQ_TIMER            0x20
#define IRQ_ERROR            0xFE
#define IRQ_BASE             0x20
#define IRQ_AP_ENTRY         0x30

#define LAPIC_PERIODIC 0x20000

#define APIC_LVT_MASKED       (1 << 16)

#define PMT_TIMER_RATE 3579545 // 3.57 MHz

void lapic_eoi();
void apic_timer_tick();
void lapic_init();
void wait_for_delivery();

uint32_t local_apic_get_id();
void lapic_write(uint32_t offset, uint32_t value);
uint32_t lapic_read(uint32_t offset);
extern uint8_t *g_localApicAddr;

void LocalApicSendStartup(uint32_t apic_id, uint8_t vector);
void LocalApicSendInit(uint32_t apic_id);

void pmt_delay(size_t us);


#endif //APIC_H
