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


#include "exec/elf.h"
#include "../include/kernel/kernel_utils.h"
#include "./cpu/cpu.h"
#include "kversion.h"
#include <log.h>
#include "kernel/scheduling.h"
#include <kernel/realm/realm_manager.h>
#include <kernel/system/system_manager.h>
#include <kernel/tty/tty.h>
#include "units/unit_manager.h"

static const char* envp0[] = {"PATH=/bin", nullptr};

extern "C" [[noreturn]] void kernel_main(BootInfo* boot_info)
{
    initialize_kernel(boot_info);
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


    RealmConfig realm_config_shell = {
        .name = "shell_realm",
        .memory_limit = 0,
        .capabilities = CAP_DEVICE_ACCESS | CAP_RW,
        .max_units = 16,
        .is_user = true,
    };
    Realm* shell_realm = RealmManager::create(&realm_config_shell);
    TTYDevice* tty_dev = kernel::tty::tty_devices[0];
    shell_realm->setup_standard_handles(tty_dev);

    ElfLoader::ElfLoadResult result = ElfLoader::load_elf_binary("/bin/shell", 0x400000, shell_realm);
    Log::Ok("Elf load result: %p", result.entry_point);
    if (!result.success)
    {
        Log::Error("Failed to load elf binary");
    }

    const char *argv_example[] = {
        "shell",
        "-v",
        "--config=config.txt",
        nullptr
    };

    UnitConfig uc = {
        .name = "shell",
        .cpu_id = 0,
        .priority = 10,
        .stack_size = 0x4000,
        .initial_handles = nullptr,
        .initial_handle_count = 0,
        .is_idle = false,
        .is_user = true,
        .user_stack_size = 0,
        .argv = argv_example,
        .envp = envp0
    };
    Unit* shell = UnitManager::create(shell_realm->id, result.entry_point, nullptr, &uc);

    Log::Ok("PF: %p %p", envp0, *envp0);

    kernel::SystemManager::set_system_initialized();

    kernel::scheduling::enable_on_cpu(0);

    while (true);
}