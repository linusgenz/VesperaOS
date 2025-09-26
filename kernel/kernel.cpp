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
#include "kversion.h"
#include "../include/log.h"
#include "acpi/acpi_manager.h"
#include "include/scheduling.h"
#include "sync/mutex.h"
#include "../filesystem/vfs/vfs.h"
#include "input/input_manager.h"
#include "realm/realm_manager.h"
#include "tty/tty.h"
#include "tty/tty.h"
#include "units/unit_manager.h"

static const char* envp0[] = {"PATH=/mnt/fat32_0/bin"};
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
    Log::Info("Kernel version: %s", get_kernel_version());
    //  kernel::time::print_current_time();
    // kernel::time::internal::sleep(5000);


    /* auto dir21 = vfs_opendir("/mnt/fat32_0/");
     Log::debug("vfs_opendir: %d", dir21);
     char name21[128];
     while (vfs_readdir(dir21, name21, sizeof(name21)) == 1) {
         Log::PrintLn("Eintrag2: %s", name21);
     }

     vfs_closedir(dir21);*/

    ElfLoader elf_loader;
    ElfLoader::ElfLoadResult result = elf_loader.load_elf_binary("/mnt/fat32_0/bin/shell.elf", 0x400000);
    if (!result.success) {
        Log::Error("Failed to load elf binary");
    }

    RealmConfig realm_config_shell = {
        .name = "shell_realm",
        .memory_limit = 0,
        .capabilities = CAP_DEVICE_ACCESS | CAP_RW,
        .max_units = 16,
        .envp = envp0,
    };
    Realm *shell_realm = RealmManager::create(&realm_config_shell);
    TTYDevice* tty_dev = kernel::tty::tty_devices[0];
    shell_realm->setup_standard_handles(tty_dev);

    UnitConfig uc = {
        .name = "shell",
        .cpu_id = 0,
        .priority = 10,
        .stack_size = DEFAULT_UNIT_STACK_SIZE,
        .initial_handles = nullptr,
        .initial_handle_count = 0,
        .is_idle = false,
        .is_user = true,
        .user_stack_size = 0
    };
    Unit *shell = UnitManager::create(shell_realm->id, (void *) result.entry_point, nullptr, &uc);

    system_initialized = true;
    kernel::scheduling::enable_on_cpu(0);

    while (true);
}
