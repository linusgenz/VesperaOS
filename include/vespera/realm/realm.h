// realm.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 19.09.25.
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

#ifndef VESPERAOS_KERNEL_REALM_H
#define VESPERAOS_KERNEL_REALM_H

#include <uapi/vespera/capabilities.h>
#include <vespera/sync/spinlock.h>
#include <vespera/sync/wait_queue.h>

#include "vespera/security/credentials.h"
#include "vespera/types.h"

class Unit;
class HandleTable;
class TtyDevice;
namespace kernel::realm {
    class AddressSpace;
}

constexpr uptr TRAMPOLINE_VADDR = 0x00007FFFFE000000ULL;
constexpr uptr TRAMP_SIGNAL_OFF = 0x000;
constexpr uptr TRAMP_UNIT_OFF = 0x100;
constexpr uptr SIGNAL_TRAMPOLINE_VADDR = (TRAMPOLINE_VADDR + TRAMP_SIGNAL_OFF);
constexpr uptr USER_UNIT_TRAMPOLINE_VADDR = (TRAMPOLINE_VADDR + TRAMP_UNIT_OFF);

#define KERNEL_REALM_SYSTEM 1
#define KERNEL_REALM_DRIVER 2

class Realm {
   public:
    RealmId id;
    char name[64]{};
    capability_set capabilities;
    u64 memory_limit;
    u64 max_units;
    u64 unit_count;

    kernel::realm::AddressSpace* address_space{nullptr};

    char cwd_path[256];

    int exit_code;
    bool exited;

    Unit* unit_list;

    WaitQueue wait_queue;

    HandleTable* handle_table;

    Spinlock lock;
    bool active;
    u8 sched_priority;
    u64 cpu_time_accumulated;

    RealmId pgid;
    RealmId sid;
    RealmId parent_id;
    TtyDevice* controlling_tty;

    kernel::security::process_credentials cred;

    Realm();
    TtyDevice* get_tty_device();
};

#endif  // VESPERAOS_KERNEL_REALM_H
