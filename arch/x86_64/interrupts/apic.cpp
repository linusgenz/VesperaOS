#include "apic.h"
#include "../../../include/log.h"
#include "../../../include/string.h"
#include "../../../kernel/acpi/acpi_manager.h"
#include "../../../kernel/scheduling/pit_legacy/pit.h"
#include "../../../drivers/io/io.h"
#include "../../../kernel/acpi/madt.h"
#include "../../../kernel/cpu/cpu_manager.h"
#include "../../../kernel/include/scheduler.h"
#include "../../../kernel/utils/panic.h"
#include "../interrupts/interrupts.h"

uint32_t lapic_read(uint32_t offset) {
    volatile uint32_t* reg = reinterpret_cast<volatile uint32_t *>(g_localApicAddr + offset);
    return *reg;
}

void lapic_write(uint32_t offset, uint32_t value) {
    volatile uint32_t* reg = reinterpret_cast<volatile uint32_t *>(g_localApicAddr + offset);
    *reg = value;
}

void wait_for_delivery() {
    // Wait for delivery to complete
    while (lapic_read(LAPIC_ICRLO) & ICR_DELIVS) {
        asm volatile("pause"); // TODO
    }
}

void lapic_init(uint8_t cpu_id)
{
    lapic_write(LAPIC_TPR, 0);

    // Logical Destination Mode
    lapic_write(LAPIC_DFR, 0xffffffff);   // Flat mode
    lapic_write(LAPIC_LDR, 0x0); // 0x01000000   // All cpus use logical id 1

    // Configure Spurious Interrupt Vector Register
    lapic_write(LAPIC_SVR, 0x100 | IRQ_SPURIOUS);

    Log::Ok("LAPIC initialized for core %u", cpu_id);

    lapic_write(LAPIC_TDCR, 0x3); // Divide by 16
    lapic_write(LAPIC_TICR, 0xFFFFFFFF);

    pmt_delay(10000); // TODO eventuell auf 1ms gehen, für mehr präzision [every 10 ms = 1 interrupt]

    uint32_t calibration = 0xffffffff - lapic_read(LAPIC_TCCR);
    lapic_write(LAPIC_TIMER, 32 | LAPIC_PERIODIC);
    lapic_write( LAPIC_TDCR, 0x3);         // 16
    lapic_write( LAPIC_TICR, calibration);

    lapic_write(LAPIC_ICRLO, 0x0); // zero this shit
    lapic_write(LAPIC_ICRHI, 0x0);
}


void pmt_delay(const size_t us){
    ACPI::FADT* fadt = ACPI::TableManager::get_fadt();

    if(fadt->pm_timer_length != 4){
        panic("ACPI Timer unavailable");
    }

    const uint64_t count = inl(fadt->pm_timer_block);
    const uint64_t target = (us*PMT_TIMER_RATE)/1000000;
    uint64_t current = 0;

    while (current < target) {
        current = ((inl(fadt->pm_timer_block) - count) & 0xffffff);
    }

}

uint32_t local_apic_get_id()
{
    return lapic_read(LAPIC_ID) >> 24;
}

volatile uint64_t apic_ticks[MAX_CPU_CORES] = {0};

void apic_timer_tick() {
    uint32_t cpu = CPUManager::get_current_cpu_id();
    apic_ticks[cpu]++;


    if (!kernel::scheduling::is_initialized()) return;

    kernel::scheduling::cpu_scheduler::wake_sleeping_threads(cpu, apic_ticks[cpu]);

    kernel::scheduling::cpu_scheduler::tick_cpu(cpu);
}


void sleep(uint64_t ms) {
    uint64_t ticks_to_wait = (ms + 9) / 10;
    uint32_t cpu = CPUManager::get_current_cpu_id();
    uint64_t target = apic_ticks[cpu] + ticks_to_wait;

    while (apic_ticks[cpu] < target) {
        asm volatile("hlt");
    }
}


void lapic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}
