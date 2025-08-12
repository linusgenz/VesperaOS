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
#include "../../include/scheduler.h"
#include "../../scheduling/thread_manager.h"

namespace syscalls::internal {
    int64_t sys_exit(uint64_t code, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
        //  Log::PrintLn("[SYS_EXIT] Code: %llu", code);
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        kthread_t *current = kernel::scheduling::get_current_thread();
        kprocess_t *proc = current->process;

        current->exit_code = (int64_t) code;
        current->state = THREAD_TERMINATED;

        if (all_threads_terminated(proc)) {
            proc->exit_code = (int64_t) code;
            proc->state = PROCESS_TERMINATED;

            cleanup_process(proc);
        }
        else {
            kernel::scheduling::thread_manager::cleanup_thread(current);
        }

        kernel::scheduling::cpu_scheduler::cpu_scheduler_t *cpu = kernel::scheduling::cpu_scheduler::get_cpu_data(cpu_id);
        cpu->current_thread = nullptr;

        kernel::scheduling::cpu_scheduler::yield_cpu(cpu_id);

        while (true) {
        }
    }
}
