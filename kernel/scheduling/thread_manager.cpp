//
// Created by Linus on 17.07.25.
//

#include "thread_manager.h"
#include "cpu_scheduler.h"
#include "../../arch/x86_64/gdt/gdt.h"
#include "../cpu/cpu_manager.h"
#include "../../include/log.h"

namespace kernel::scheduling::thread_manager {
    extern "C" [[noreturn]] void idle_thread_func(void *arg) {
        Log::Info("entered IDLE thread");
        uint32_t cpu_id = CPUManager::get_current_cpu_id();

        while (true) {
            __asm__ volatile ("pause");

            cpu_scheduler::cpu_scheduler_t *cpu = cpu_scheduler::get_cpu_data(cpu_id);
            if (cpu->ready_queue_head) {
                cpu_scheduler::yield_cpu(cpu_id);
            }
        }
    }

    void add_thread(kthread_t *thread) {
        if (!thread) return;

        uint8_t cpu_id = thread->cpu_id;
        cpu_scheduler::add_thread_to_cpu(thread, cpu_id);
    }

    void remove_thread(kthread_t *thread) {
        if (!thread) return;

        uint8_t cpu_id = thread->cpu_id;
        cpu_scheduler::remove_thread_from_cpu(thread, cpu_id);
    }

    void cleanup_thread(kthread_t *thread) {
        if (!thread || thread->is_idle_thread) return;

        kernel::memory::free_pages(thread->stack, 2);
        kernel::memory::free_page(thread);
    }

    kthread_t *get_current_thread() {
        uint32_t cpu_id = CPUManager::get_current_cpu_id();
        return cpu_scheduler::get_current_thread_on_cpu(cpu_id);
    }

    static inline void wrmsr(uint32_t msr, uint64_t value) {
        uint32_t low = (uint32_t) (value & 0xFFFFFFFF);
        uint32_t high = (uint32_t) (value >> 32);
        asm volatile ("wrmsr" :: "c"(msr), "a"(low), "d"(high));
    }

#define MSR_KERNEL_GS_BASE 0xC0000102
#define MSR_GS_BASE 0xC0000101

    void switch_to_thread(kthread_t *from, kthread_t *to, interrupt_frame *frame) {
        int to_is_user = to->is_user_thread ? 1 : 0;

        int from_is_user = from->is_user_thread ? 1 : 0;
        if (to_is_user) {
            //wrmsr(MSR_GS_BASE, 0);
            //wrmsr(MSR_KERNEL_GS_BASE, (uint64_t) &to);
        }


        if (from != nullptr && from->state == THREAD_TERMINATED) {
            cleanup_thread(from);
            context_switch(nullptr, to->stack_pointer, to_is_user, 0, frame);
        } else {
            auto ptr1 = from ? &from->stack_pointer : nullptr;

            context_switch(ptr1, to->stack_pointer, to_is_user, from_is_user, frame);
        }
    }

    [[noreturn]] void terminate_current_thread() {
        asm volatile("cli");
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        cpu_scheduler::cpu_scheduler_t *cpu = cpu_scheduler::get_cpu_data(cpu_id);

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

    extern "C" void switch_to_user_mode(void *user_stack_top, void *user_code_virt);

    extern "C" void thread_trampoline() {
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        kthread_t *current = cpu_scheduler::get_current_thread_on_cpu(cpu_id);

        asm volatile("sti");

        if (current->is_user_thread) {
            //      current->user_entry();

            //    switch_to_user_mode(current->user_stack_top, current->user_entry);
            //      __builtin_unreachable();
        }

        current->entry(current->arg);

        Log::LogMsg("exiting thread %u", current->id);
        terminate_current_thread();
    }


    void setup_idle_thread(uint8_t cpu_id) {
        cpu_scheduler::cpu_scheduler_t *cpu = cpu_scheduler::get_cpu_data(cpu_id);
        cpu->idle_thread = create_idle_kthread(idle_thread_func, cpu_id);
    }
} // namespace kernel::scheduling::thread_manager
