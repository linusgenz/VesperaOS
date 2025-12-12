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

#include "../include/kernel/devices/device_manager.h"
#include "units/unit_manager.h"

static const char* envp0[] = {"PATH=/bin", nullptr};

static const char* dev_type_to_str(DeviceType t)
{
    switch (t) {
        case DeviceType::Block: return "Block";
        case DeviceType::Char: return "Char";
        case DeviceType::Controller: return "Controller";
        case DeviceType::Bus: return "Bus";
        case DeviceType::Logical: return "Logical";
        default: return "Other";
    }
}

static const char* class_to_str(DeviceClass c)
{
    switch (c) {
        case DeviceClass::Storage: return "Storage";
        case DeviceClass::Usb:     return "USB";
        case DeviceClass::Input:   return "Input";
        case DeviceClass::Net:     return "Net";
        case DeviceClass::Misc:    return "Misc";
        case DeviceClass::Pseudo:  return "Pseudo";
        default: return "Unknown";
    }
}

static const char* bus_to_str(BusType b)
{
    switch (b) {
        case BusType::BUS_NONE:  return "None";
        case BusType::BUS_PCI:   return "PCI";
        case BusType::BUS_USB:   return "USB";
        case BusType::BUS_PS2:    return "PS2";
        case BusType::VIRTUAL:  return "Virtual";
        default:        return "Other";
    }
}

// Pretty-print indentation tree
static void print_indent(int depth)
{
    for (int i = 0; i < depth; i++) {
        Log::Print("  "); // 2 spaces per level
    }
}

static void print_device_tree(KernelDevice* dev, int depth = 0)
{
    if (!dev) return;

    print_indent(depth);

    Log::PrintLn(
        "[id=%u] %s (%s, class=%s, bus=%s)",
        dev->id,
        dev->name ? dev->name : "<noname>",
        dev_type_to_str(dev->type),
        class_to_str(dev->dev_class),
        bus_to_str(dev->bus_type)
    );

    // extra info
    if (dev->block && dev->type == DeviceType::Block) {
        print_indent(depth + 1);
        Log::PrintLn("BlockDevice: sector=%u",
                     dev->block->get_sector_size());
    }

    // recursively print children
    for (auto* child : dev->children) {
        print_device_tree(child, depth + 1);
    }
}

void Debug_PrintAllDevices()
{
    using namespace kernel;

    auto list = DeviceManager::GetAllDevices();

    Log::PrintLn("=== Registered Devices (%u) ===",
                 DeviceManager::GetKernelDeviceCount());

    // Only print root-level devices (those without parent)
    for (auto* dev : list)
    {
        if (dev && dev->parent == nullptr)
        {
            print_device_tree(dev, 0);
        }
    }

    Log::PrintLn("==============================");
}


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

    kernel::SystemManager::set_system_initialized();

    Debug_PrintAllDevices();

  //  kernel::scheduling::enable_on_cpu(0);

    while (true);
}