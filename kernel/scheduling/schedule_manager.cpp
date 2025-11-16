//
// Created by Linus on 17.07.25.
//

#include "schedule_manager.h"
#include "cpu_scheduler.h"
#include "../../arch/x86_64/gdt/gdt.h"
#include "../cpu/cpu_manager.h"
#include "../../include/log.h"
#include "../realm/realm_manager.h"
#include "../units/unit_manager.h"

namespace kernel::scheduling::manager {
    extern "C" [[noreturn]] void idle_unit_func(void *arg) {
        while (true) {
            __asm__ volatile ("sti; hlt");
        }
    }

    static inline void wrmsr(uint32_t msr, uint64_t value) {
        uint32_t low = (uint32_t) (value & 0xFFFFFFFF);
        uint32_t high = (uint32_t) (value >> 32);
        asm volatile ("wrmsr" :: "c"(msr), "a"(low), "d"(high));
    }

#define MSR_KERNEL_GS_BASE 0xC0000102
#define MSR_GS_BASE 0xC0000101

    void switch_to_unit(Unit *from, Unit *to, interrupt_frame *frame) {
        const bool from_syscall = from && from->context.from_syscall;

        if (to->is_user) {
            wrmsr(MSR_GS_BASE, 0);
            auto *ctx_ptr = &to->context;
            wrmsr(MSR_KERNEL_GS_BASE, (uint64_t) &ctx_ptr);
            if (to->rid) {
                Realm *r = RealmManager::get(to->rid);
                uint64_t cr3 = (uint64_t) r->pml4;
                asm volatile("mov %0, %%cr3" :: "r"(cr3));
            }
        } else {
            asm volatile("mov %0, %%cr3" :: "r"(memory::get_pagetable_address()));
        }

        arg_registers_t *push_args = nullptr;
        void **rdi_save_addr = nullptr;
        void *rsp_to_load = nullptr;
        uint64_t should_iretq = 0;
        uint64_t save_iretq = 0;
        int frame_ptr = 0;

        if (from_syscall) {
            rdi_save_addr = &from->context.stack_pointer;
            frame_ptr = 0;
        } else if (from) {
            rdi_save_addr = &from->context.stack_pointer;
            // frame_ptr = frame;
            if (from->is_user) {
                save_iretq = 1;
                rdi_save_addr = nullptr;
                from->context.stack_pointer = frame;
            }
        }


        if (to->context.from_syscall) {
            asm volatile("swapgs");
            // todo, when a sleep thread gets waken up the gs points to the user gs for some reason. swap to kernel gs so we dont mess up things
            rsp_to_load = (void *) to->context.stack_pointer;
            frame_ptr = 0;
            should_iretq = 0;
        } else {
            rsp_to_load = to->context.stack_pointer;
            if (to->is_user) {
                //  frame_ptr = frame;
                should_iretq = 1;
                frame_ptr = 0;
            }
        }

        if (to->is_user && !to->context.initialized) {
            to->context.initialized = true;
            push_args = &to->context.regs;
        }

        context_switch(rdi_save_addr, rsp_to_load, should_iretq, save_iretq, push_args);
    }

    [[noreturn]] void terminate_current_thread() {
        asm volatile("cli");
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        cpu_scheduler::cpu_scheduler_t *cpu = cpu_scheduler::get_cpu_data(cpu_id);

        if (!cpu->current_unit || cpu->current_unit->is_idle) {
            while (true) {
                asm volatile("hlt");
            }
        }

        cpu_scheduler::lock_ready_queue(cpu_id);
        cpu->current_unit->state = UNIT_TERMINATED;
        cpu_scheduler::unlock_ready_queue(cpu_id);

        cpu_scheduler::yield_cpu(cpu_id);

        while (true) {
            asm volatile("hlt");
        }
    }

    extern "C" void unit_trampoline() {
        const uint8_t cpu_id = CPUManager::get_current_cpu_id();
        const Unit *current = cpu_scheduler::get_current_unit_on_cpu(cpu_id);

        current->context.entry(current->context.arg);
        Log::debug("kUnit end %u", current->id);
        terminate_current_thread();
    }


    Unit *setup_idle_unit(const uint8_t cpu_id) {
        const UnitConfig unit_config = {
            .name = "idle_thread",
            .cpu_id = cpu_id,
            .priority = PRIORITY_NONE,
            .stack_size = 0x1000,
            .initial_handles = nullptr,
            .initial_handle_count = 0,
            .is_idle = true,
            .is_user = false,
            .user_stack_size = 0,
        };

        return UnitManager::create(KERNEL_REALM_SYSTEM, (void *) idle_unit_func, nullptr, &unit_config);
    }
} // namespace kernel::scheduling::manager
