//
// Created by linus on 02.07.25.
//
#include "time.h"
#include "../../arch/x86_64/interrupts/apic.h"
#include "../../include/log.h"
#include "../cpu/cpu_manager.h"
#include "../include/page_frame_allocator.h"
#include "../scheduling/scheduler.h"
#include "../scheduling/thread.h"

extern volatile uint64_t apic_ticks[MAX_CPU_CORES];

namespace kernel::time {
    void thread_sleep_ms(uint64_t ms) {
        uint32_t cpu_id = CPUManager::get_current_cpu_id();
        kernel::scheduling::cpu_scheduler_t *cpu = &kernel::scheduling::global_scheduler.cpus[cpu_id];

        kernel::scheduling::lock(&cpu->lock);

        kthread_t *current = cpu->current_thread;
        if (!current) {
            kernel::scheduling::unlock(&cpu->lock);
            return;
        }

        current->wakeup_tick = apic_ticks[cpu_id] + (ms + 9) / 10;
        current->state = THREAD_BLOCKED;

        if (!cpu->blocked_queue_head) {
            cpu->blocked_queue_head = current;
            current->next = nullptr;
        } else {
            kthread_t *iter = cpu->blocked_queue_head;
            while (iter->next && iter->next->wakeup_tick <= current->wakeup_tick) {
                iter = iter->next;
            }

            current->next = iter->next;
            iter->next = current;

        }

        kernel::scheduling::unlock(&cpu->lock);
        kernel::scheduling::yield();
    }

    void sleep_ms(uint64_t ms) {
        uint32_t cpu = CPUManager::get_current_cpu_id();
        uint64_t target = apic_ticks[cpu] + (ms + 9) / 10;
        while (apic_ticks[cpu] < target) {
            asm volatile("hlt");
        }
    }

    uint64_t get_ticks() {
        uint32_t cpu = CPUManager::get_current_cpu_id();
        return apic_ticks[cpu];
    }

    uint64_t get_uptime_ms() {
        return get_ticks() * 10;
    }
}
