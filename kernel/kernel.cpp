// kernel.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 02.08.25.
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

#include "throbber.h"
#include "exec/elf.h"
#include "./include/kernel_utils.h"
#include "./cpu/cpu.h"
#include "time/time.h"
#include "version.h"
#include "include/sys/syscalls.h"
#include "../include/log.h"
#include "acpi/acpi_manager.h"
#include "include/scheduling.h"
#include "sync/mutex.h"


extern "C" [[noreturn]] void kernel_main(BootInfo *boot_info) {
    system_initialized = false;
    initialize_kernel(boot_info);
    kernel::scheduling_started = true;
    char vendor[13];
    get_cpu_vendor(vendor);
  //  Log::Info("CPU Vendor: %s", vendor);

    char brand[49];
    get_cpu_brand(brand);
  //  Log::Info("CPU Brand: %s", brand);
    Log::Ok("Kernel initialized successfully");
  //  Log::Info("Kernel version: %s", get_os_version());
  //  kernel::time::print_current_time();

  //  kprocess_t *shell_proc = create_process_from_elf("shell", "/mnt/fat32_0/bin/shell.elf");
 //   shell_proc->state = PROCESS_READY;
 //   shell_proc->main_thread->state = THREAD_READY;
 //   kernel::scheduling::add_thread(shell_proc->main_thread);

    system_initialized = true;
  //  kernel::scheduling::enable_on_cpu(0);

    while (true);
}
