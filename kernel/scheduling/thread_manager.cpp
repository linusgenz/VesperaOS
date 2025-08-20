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
        while (true) {
            __asm__ volatile ("sti; hlt");
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
        const bool to_is_user = to->is_user_thread;
        const bool from_syscall = from && from->from_syscall;
        const bool from_is_user = from && from->is_user_thread;
        const bool to_syscall = to->from_syscall;

        if (to->is_user_thread) {
            wrmsr(MSR_GS_BASE, 0);
            wrmsr(MSR_KERNEL_GS_BASE, (uint64_t) &to);
            if (to->process) {
                uint64_t cr3 = (uint64_t) to->process->pml4;
                asm volatile("mov %0, %%cr3" :: "r"(cr3));
            }
        } else {
            asm volatile("mov %0, %%cr3" :: "r"(memory::get_pagetable_address()));
        }

        void **rdi_save_addr = nullptr;
        void *rsp_to_load = nullptr;
        uint64_t should_iretq = 0;
        uint64_t save_iretq = 0;
        int frame_ptr = 0;

        if (from_syscall) {
            rdi_save_addr = &from->stack_pointer;
            frame_ptr = 0;
        } else if (from) {
            rdi_save_addr = &from->stack_pointer;
           // frame_ptr = frame;
            if (from_is_user) {
                save_iretq = 1;
                rdi_save_addr = nullptr;
                from->stack_pointer = frame;
            }
        }


        if (to_syscall) {
            asm volatile("swapgs");
            // todo, when a sleep thread gets waken up the gs points to the user gs for some reason. swap to kernel gs so we dont mess up things
            rsp_to_load = (void *) to->stack_pointer;
            frame_ptr = 0;
            should_iretq = 0;
        } else {
            rsp_to_load = to->stack_pointer;
            if (to_is_user) {
              //  frame_ptr = frame;
                should_iretq = 1;
                frame_ptr =0;
            }
        }

        context_switch(rdi_save_addr, rsp_to_load, should_iretq, save_iretq, frame_ptr);
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

    extern "C" void thread_trampoline() {
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        kthread_t *current = cpu_scheduler::get_current_thread_on_cpu(cpu_id);

        current->entry(current->arg);

        terminate_current_thread();
    }


    void setup_idle_thread(uint8_t cpu_id) {
        cpu_scheduler::cpu_scheduler_t *cpu = cpu_scheduler::get_cpu_data(cpu_id);
        cpu->idle_thread = create_idle_kthread(idle_thread_func, cpu_id);
    }
} // namespace kernel::scheduling::thread_manager
