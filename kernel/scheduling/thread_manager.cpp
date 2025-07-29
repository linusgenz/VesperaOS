//
// Created by Linus on 17.07.25.
//

#include "thread_manager.h"
#include "cpu_scheduler.h"
#include "../cpu/cpu_manager.h"
#include "../../include/log.h"

namespace kernel::scheduling::thread_manager {

    extern "C" [[noreturn]] void idle_thread_func(void* arg) {
        uint32_t cpu_id = CPUManager::get_current_cpu_id();

        while (true) {
            __asm__ volatile ("pause");

            cpu_scheduler::cpu_scheduler_t* cpu = cpu_scheduler::get_cpu_data(cpu_id);
            if (cpu->ready_queue_head) {
                cpu_scheduler::yield_cpu(cpu_id);
            }
        }
    }

    void add_thread(kthread_t* thread) {
        if (!thread) return;

        uint8_t cpu_id = thread->cpu_id;
        cpu_scheduler::add_thread_to_cpu(thread, cpu_id);
    }

    void remove_thread(kthread_t* thread) {
        if (!thread) return;

        uint8_t cpu_id = thread->cpu_id;
        cpu_scheduler::remove_thread_from_cpu(thread, cpu_id);
    }

    void cleanup_thread(kthread_t* thread) {
        if (!thread || thread->is_idle_thread) return;

        kernel::memory::free_pages(thread->stack, 2);
        kernel::memory::free_page(thread);
    }

    kthread_t* get_current_thread() {
        uint32_t cpu_id = CPUManager::get_current_cpu_id();
        return cpu_scheduler::get_current_thread_on_cpu(cpu_id);
    }

    void switch_to_thread(kthread_t* from, kthread_t* to) {
        if (from != nullptr && from->state == THREAD_TERMINATED) {
            cleanup_thread(from);
            context_switch(nullptr, to->stack_pointer);
        } else {
            auto ptr1 = from ? &from->stack_pointer : nullptr;
            context_switch(ptr1, to->stack_pointer);
        }
    }

    [[noreturn]] void terminate_current_thread() {
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        cpu_scheduler::cpu_scheduler_t* cpu = cpu_scheduler::get_cpu_data(cpu_id);

        if (!cpu->current_thread || cpu->current_thread->is_idle_thread) {
            while (true) {
                asm volatile("hlt");
            }
        }

        cpu_scheduler::lock_ready_queue(cpu_id);
        cpu->current_thread->state = THREAD_TERMINATED;
        cpu_scheduler::unlock_ready_queue(cpu_id);

        cpu_scheduler::yield_cpu(cpu_id);

        while (true) {
            asm volatile("hlt");
        }
    }

    extern "C" void thread_trampoline() {
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        kthread_t* current = cpu_scheduler::get_current_thread_on_cpu(cpu_id);

        asm volatile("sti");
        current->entry(&current->arg);
        Log::LogMsg("exiting thread %u", current->id);
        terminate_current_thread();
    }

    void setup_idle_thread(uint8_t cpu_id) {
        cpu_scheduler::cpu_scheduler_t* cpu = cpu_scheduler::get_cpu_data(cpu_id);
        cpu->idle_thread = create_idle_kthread(idle_thread_func, cpu_id);
    }

} // namespace kernel::scheduling::thread_manager