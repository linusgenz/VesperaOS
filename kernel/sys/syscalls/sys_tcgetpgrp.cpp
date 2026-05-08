// sys_tcgetpgrp.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 23.04.26.
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
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/tty/tty.h>
#include <vespera_errno.h>

#include "../../tty/tty_device.h"

namespace syscalls::internal {
    i64 sys_tcgetpgrp(u64 arg0, u64, u64, u64, u64, u64) {
        const Unit* caller = kernel::scheduling::get_current_unit();
        if (!caller) return -ESRCH;

        Realm* self = caller->parent;
        if (!self) return -ESRCH;

        const HandleId hid = arg0;

        HandleEntry* he = self->handle_table.lookup(hid);
        if (!he || he->type != HANDLE_TYPE_TTY) return -ENOTTY;

        const auto* tty_dev = static_cast<TtyDevice*>(he->resource);
        if (!tty_dev || !tty_dev->tty) return -ENOTTY;

        return static_cast<i64>(tty_dev->tty->fg_pgid);
    }
}  // namespace syscalls::internal
