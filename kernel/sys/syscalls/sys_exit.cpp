// sys_exit.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.08.25.
//
// This file is part of VesperaOS.
// 
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include "cstdint"
#include "../../../include/log.h"
#include "../../cpu/cpu_manager.h"
#include <scheduling.h>

namespace syscalls::internal {
    int64_t sys_exit(uint64_t code, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
        Log::PrintLn("SYS EXIT IS NOT BUILD");
       /* uint8_t cpu_id = CPUManager::get_current_cpu_id();
        kthread_t *current = kernel::scheduling::get_current_thread();
        kprocess_t *proc = current->process;

        current->exit_code = (int64_t) code;
        current->state = THREAD_TERMINATED;

        asm volatile("mov %0, %%cr3" :: "r"(kernel::memory::get_pagetable_address()));

        if (kernel::process::Manager::all_threads_from_proc_terminated(proc)) {
            proc->exit_code = (int64_t) code;
            proc->state = PROCESS_TERMINATED;

            kernel::process::Manager::cleanup_process(proc);
        }
        else {
            kernel::scheduling::remove_thread(current);
            kernel::threading::ThreadFactory::cleanup_thread_resources(current);
            // later release tid of thread
        }


        kernel::scheduling::cpu_scheduler::cpu_scheduler_t *cpu = kernel::scheduling::cpu_scheduler::get_cpu_data(cpu_id);
        cpu->current_thread = nullptr;

        kernel::scheduling::cpu_scheduler::yield_cpu(cpu_id);

        for (;;) { asm volatile("hlt"); }*/
    }
}
