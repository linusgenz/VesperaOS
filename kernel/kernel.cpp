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
#include "include/time.h"
#include "version.h"
#include "include/sys/syscalls.h"
#include "../include/log.h"
#include "acpi/acpi_manager.h"
#include "include/scheduling.h"
#include "proc/process_manager.h"
#include "sync/mutex.h"

#include "../filesystem/vfs/vfs.h"
#include "input/input_manager.h"
#include "threading/threading.h"
#include "tty/tty.h"

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
    kernel::time::internal::sleep(5000);



    /* auto dir21 = vfs_opendir("/mnt/fat32_0/");
     Log::debug("vfs_opendir: %d", dir21);
     char name21[128];
     while (vfs_readdir(dir21, name21, sizeof(name21)) == 1) {
         Log::PrintLn("Eintrag2: %s", name21);
     }

     vfs_closedir(dir21);*/


 /*   kernel::process::ProcessCreateOptions options_shell = {
        .name = "shell",
        .priority = 0,
        .cpu_id = 0,
        .heap_start = 0,
        .heap_size = 0,
        .stack_size = 0x3000,
        .is_kernel_process = false,
        .custom_pml4 = nullptr,
    };*/
  //  kprocess_t *shell_proc = PROCESS_MANAGER::create_process_from_elf(options_shell, "/mnt/fat32_0/bin/shell.elf");



    system_initialized = true;
    kernel::scheduling::enable_on_cpu(0);

    while (true);
}
