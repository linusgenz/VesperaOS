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

#include <vespera/boot/boot.h>
#include <vespera/devices/device_manager.h>
#include <vespera/kernel_utils.h>
#include <vespera/log.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/system/system_manager.h>
#include <vespera/tty/tty.h>
#include <vespera/types.h>

#include "./cpu/cpu.h"
#include "exec/elf.h"
#include "kversion.h"
#include "scheduling/per_cpu.h"
#include "units/unit_manager.h"
#include "vespera/scheduling.h"

static const char* envp0[] = {"PATH=/bin", "TERM=tty0", nullptr};

static const char* dev_type_to_str(DeviceType t) {
    switch (t) {
        case DeviceType::Block:
            return "Block";
        case DeviceType::Char:
            return "Char";
        case DeviceType::Controller:
            return "Controller";
        case DeviceType::Bus:
            return "Bus";
        case DeviceType::Logical:
            return "Logical";
        default:
            return "Other";
    }
}

static const char* class_to_str(DeviceClass c) {
    switch (c) {
        case DeviceClass::Storage:
            return "Storage";
        case DeviceClass::Usb:
            return "USB";
        case DeviceClass::Input:
            return "Input";
        case DeviceClass::Net:
            return "Net";
        case DeviceClass::Misc:
            return "Misc";
        case DeviceClass::Pseudo:
            return "Pseudo";
        default:
            return "Unknown";
    }
}

static const char* bus_to_str(BusType b) {
    switch (b) {
        case BusType::None:
            return "None";
        case BusType::Pci:
            return "PCI";
        case BusType::Usb:
            return "USB";
        case BusType::Ps2:
            return "PS2";
        case BusType::VIRTUAL:
            return "Virtual";
        default:
            return "Other";
    }
}

// Pretty-print indentation tree
static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        Log::print("  ");  // 2 spaces per level
    }
}

static void print_device_tree(KernelDevice* dev, int depth = 0) {
    if (!dev) return;

    print_indent(depth);

    Log::print_ln(
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
        Log::print_ln("BlockDevice: sector=%u", dev->block->get_sector_size());
    }

    // recursively print children
    for (auto* child : dev->children) {
        print_device_tree(child, depth + 1);
    }
}

void debug_print_all_devices() {
    using namespace kernel;

    auto list = DeviceManager::get_all_devices();

    Log::print_ln("=== Registered Devices (%u) ===", DeviceManager::get_kernel_device_count());

    // Only print root-level devices (those without parent)
    for (auto* dev : list) {
        if (dev && dev->parent == nullptr) {
            print_device_tree(dev, 0);
        }
    }

    Log::print_ln("==============================");
}

extern "C" [[noreturn]] void kernel_main(BootInfo* boot_info) {
    Log::disable_debug();
    initialize_kernel(boot_info);
    char vendor[13];
    get_cpu_vendor(vendor);
    Log::info("CPU Vendor: %s", vendor);
    char brand[49];
    get_cpu_brand(brand);
    Log::info("CPU Brand: %s", brand);
    Log::ok("Kernel initialized successfully");
    Log::info("Kernel version: %s", get_kernel_version());

    constexpr RealmConfig realm_config_shell = {
        .name = "initium",
        .memory_limit = 0,
        .capabilities = CAP_DEVICE_ACCESS | CAP_RW,
        .max_units = 16,
        .is_user = true,
    };
    Realm* shell_realm = RealmManager::create(&realm_config_shell);
    TtyDevice* tty_dev = kernel::tty::tty_devices[0];
    shell_realm->setup_standard_handles(tty_dev);

    const ElfLoader::LoadResult result = ElfLoader::load("/bin/lua", 0x400000, shell_realm);
    if (!result.success) {
        Log::error("Failed to load elf binary: %s", result.error_message);
    }

    const char* argv_init[] = {"lua", "/etc/init.lua", nullptr};
    const char* envp_init[] = {"PATH=/bin", "LUA_PATH=/etc/lib/?.lua", nullptr};

    const UnitConfig uc = {
        .name = "init",
        .cpu_id = 0,
        .priority = 10,
        .stack_size = DEFAULT_UNIT_STACK_SIZE,
        .initial_handles = nullptr,
        .initial_handle_count = 0,
        .is_idle = false,
        .is_user = true,
        .is_main_unit = true,
        .user_stack_size = 0,
        .argv = argv_init,
        .envp = envp_init
    };

    if (Unit* shell_unit =
            UnitManager::create(shell_realm->id, reinterpret_cast<unit_entry_t>(result.entry_point), nullptr, &uc)) {
        const uptr heap_begin = (result.load_end + 0xFFFULL) & ~0xFFFULL;
        shell_unit->heap_start = heap_begin;
        shell_unit->heap_end = heap_begin;
    }

    kernel::SystemManager::set_system_initialized();
    kernel::SystemManager::get_system_terminal()->set_cursor_visible(true);
    //  Debug_PrintAllDevices();
    per_cpu_init(0);
    kernel::scheduling::enable_on_cpu(0);

    while (true);
}