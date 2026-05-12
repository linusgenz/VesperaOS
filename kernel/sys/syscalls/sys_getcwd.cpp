// sys_getcwd.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.03.26.
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

#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera_errno.h>

#include <kernel/units/unit.h>

namespace syscalls::internal {
    i64 sys_getcwd(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const auto buf = reinterpret_cast<char*>(arg0);
        const usize size = arg1;

        if (!buf || size == 0) return -EINVAL;

        const Unit* cur = kernel::scheduling::get_current_unit();
        if (!cur) return -EINVAL;

        const Realm* realm = cur->parent;
        if (!realm) return -EINVAL;

        const usize len = strlen(realm->cwd_path);
        if (len + 1 > size) return -ERANGE;

        memcpy(buf, realm->cwd_path, len + 1);

        return static_cast<i64>(len + 1);
    }
}