#include "apic.h"

#include "../../../include/string.h"
#include "../../../kernel/acpi/acpi_manager.h"
#include "../../../kernel/include/basic_renderer.h"
#include "../../../kernel/scheduling/pit/pit.h"


uint32_t LAPIC_ADDRESS = 0;

uint32_t lapic_read(uint32_t offset) {
    volatile uint32_t* reg = reinterpret_cast<volatile uint32_t *>(LAPIC_ADDRESS + offset);
    return *reg;
}

void lapic_write(uint32_t offset, uint32_t value) {
    volatile uint32_t* reg = reinterpret_cast<volatile uint32_t *>(LAPIC_ADDRESS + offset);
    *reg = value;
}

void lapic_init()
{
    // Clear task priority to enable all interrupts
    lapic_write(LAPIC_TPR, 0);

    // Logical Destination Mode
    lapic_write(LAPIC_DFR, 0xffffffff);   // Flat mode
    lapic_write(LAPIC_LDR, 0x01000000);   // All cpus use logical id 1

    // Configure Spurious Interrupt Vector Register
    lapic_write(LAPIC_SVR, 0x100 | IRQ_SPURIOUS);

    global_renderer->print("LAPIC initialized");

    lapic_write(LAPIC_TDCR, 0x3); // Divide by 16
    lapic_write(LAPIC_TICR, 0xFFFFFFFF);
   // lapic_write(LAPIC_TIMER, IRQ_TIMER | 0x00b);

    pmt_delay(10000); // TODO eventuell auf 1ms gehen, für mehr präzision

  //  lapic_write(LAPIC_TDCR, 0x10000);     // 0x10000 = masked, sdm
    uint32_t calibration = 0xffffffff - lapic_read(LAPIC_TCCR);
    lapic_write(LAPIC_TIMER, 32 | LAPIC_PERIODIC);
    lapic_write( LAPIC_TDCR, 0x3);         // 16
    lapic_write( LAPIC_TICR, calibration);
  //  lapic_write(LAPIC_TICR, 1000000); // 128/16 = Faktor 8
}

void pmt_delay(const size_t us){
    ACPI::FADT* fadt = ACPI::TableManager::get_fadt();

    if(fadt->pm_timer_length != 4){

        global_renderer->print("ACPI Timer unavailable"); // panic for now
    }

    const uint64_t count = inl(fadt->pm_timer_block);
    const uint64_t target = (us*PMT_TIMER_RATE)/1000000;
    uint64_t current = 0;

    while (current < target) {
        current = ((inl(fadt->pm_timer_block) - count) & 0xffffff);
    }

}

uint32_t LocalApicGetId()
{
    return lapic_read(LAPIC_ID) >> 24;
}


volatile uint64_t apic_ticks = 0;

void apic_timer_tick() {
    apic_ticks++;
}

void sleep(uint64_t ms) {
    uint64_t ticks_to_wait = (ms + 9) / 10;
    uint64_t target = apic_ticks + ticks_to_wait;

    while (apic_ticks < target) {
        asm volatile("hlt");
    }
}

// cant remove this shit, when i do i get a page fault lol, idk why
void sleep_ms(uint32_t ms) {
    // Umrechnung auf PIT-Ticks (1.193182 MHz) → 1 ms ≈ 1193 Ticks
    uint16_t count = (1193182 * ms);

    if (count == 0)
        count = 1;

    // PIT Channel 2: Mode 0 (interrupt on terminal count), binary, lobyte/hibyte
    outb(0x43, 0b10110000); // Channel 2, Mode 0, 16-bit binary, lobyte/hibyte
    outb(0x42, count & 0xFF);         // Low byte
    outb(0x42, (count >> 8) & 0xFF);  // High byte

    // Gate aktivieren (Port 0x61, Bit 0 = 1)
    uint8_t tmp = inb(0x61);
    outb(0x61, (tmp & 0xFC) | 1);
    outb(0x61, tmp | 1);

    // Warte auf OUT-Pin (Bit 5 von Port 0x61) → high = fertig
    while (!(inb(0x61) & 0x20)) {
        asm volatile("pause");
    }
}


void lapic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}
