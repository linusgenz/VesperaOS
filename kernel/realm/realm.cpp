// realm.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 28.11.25.
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

#include <uapi/vespera/handles.h>
#include <vespera/realm/realm.h>

#include <realm/handle_table.h>
#include "klib/string.h"

Realm::Realm()
    : id(0)
    , capabilities(CAP_NONE)
    , memory_limit(0)
    , max_units(0)
    , unit_count(0)
    , cwd_path{}
    , exit_code(0)
    , exited(false)
    , unit_list(nullptr)
    , active(false)
    , sched_priority(0)
    , cpu_time_accumulated(0)
    , pgid(0)
    , sid(0)
    , parent_id(0)
    , controlling_tty(nullptr)
    , handle_table(new HandleTable()) {
    char buf[50];
    snprintf(buf, sizeof(buf), "realm_%s:%u_lock", name, id);
    lock.init(buf);
}

TtyDevice* Realm::get_tty_device() {
    const HandleEntry* he = handle_table->lookup(HANDLE_STDIN);
    if (!he || he->type != HANDLE_TYPE_TTY) return nullptr;
    return static_cast<TtyDevice*>(he->resource);
}