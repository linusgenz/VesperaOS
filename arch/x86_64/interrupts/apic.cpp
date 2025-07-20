#include "apic.h"
#include "../../../include/log.h"
#include "../../../include/string.h"
#include "../../../kernel/acpi/acpi_manager.h"
#include "../../../kernel/scheduling/pit_legacy/pit.h"
#include "../../../drivers/io/io.h"
#include "../../../kernel/acpi/madt.h"
#include "../../../kernel/cpu/cpu_manager.h"
#include "../../../kernel/include/page_frame_allocator.h"
#include "../../../kernel/scheduling/scheduler.h"
#include "../../../kernel/utils/panic.h"

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
    //    asm volatile("pause"); // TODO
    }
}

void lapic_init(uint8_t cpu_id)
{
    // Clear task priority to enable all interrupts
    lapic_write(LAPIC_TPR, 0);

    // Logical Destination Mode
    lapic_write(LAPIC_DFR, 0xffffffff);   // Flat mode
    lapic_write(LAPIC_LDR, 0x01000000);   // All cpus use logical id 1

    // Configure Spurious Interrupt Vector Register
    lapic_write(LAPIC_SVR, 0x100 | IRQ_SPURIOUS);

    Log::Ok("LAPIC initialized for core %u", cpu_id);

    lapic_write(LAPIC_TDCR, 0x3); // Divide by 16
    lapic_write(LAPIC_TICR, 0xFFFFFFFF);
   // lapic_write(LAPIC_TIMER, IRQ_TIMER | 0x00b);

    pmt_delay(10000); // TODO eventuell auf 1ms gehen, für mehr präzision [every 10 ms = 1 interrupt]

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

        Log::Error("ACPI Timer unavailable"); // panic for now
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

    kernel::scheduling::cpu_scheduler_t* cpu_sched = &kernel::scheduling::global_scheduler.cpus[cpu];
    kernel::scheduling::lock(&cpu_sched->lock);

    kthread_t* prev = nullptr;
    kthread_t* thread = cpu_sched->blocked_queue_head;

    while (thread) {
        if (apic_ticks[cpu] >= thread->wakeup_tick) {
            // Wecke Thread
            kthread_t* to_wake = thread;
            if (prev) {
                prev->next = thread->next;
            } else {
                cpu_sched->blocked_queue_head = thread->next;
            }
            thread = thread->next;

            to_wake->state = THREAD_READY;
            to_wake->next = nullptr;

            // In ready queue einfügen
            if (cpu_sched->ready_queue_tail) {
                cpu_sched->ready_queue_tail->next = to_wake;
                cpu_sched->ready_queue_tail = to_wake;
            } else {
                cpu_sched->ready_queue_head = to_wake;
                cpu_sched->ready_queue_tail = to_wake;
            }
        } else {
            prev = thread;
            thread = thread->next;
        }
    }

    kernel::scheduling::unlock(&cpu_sched->lock);
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
