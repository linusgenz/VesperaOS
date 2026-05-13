// sys_setsid.cpp
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

#include <vespera/realm/realm.h>
#include <vespera/scheduling.h>
#include <vespera_errno.h>

namespace syscalls::internal {
    i64 sys_setsid(u64, u64, u64, u64, u64, u64) {
        Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;

        if (r->sid == r->id) return -EPERM;

        r->controlling_tty = nullptr;

        r->sid  = r->id;
        r->pgid = r->id;

        return static_cast<i64>(r->id);
    }
}  // namespace syscalls::internal
