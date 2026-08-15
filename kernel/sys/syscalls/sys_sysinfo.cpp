// sys_sysinfo.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.08.26.
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

#include <vespera_errno.h>
#include "../user_copy.h"
#include <uapi/vespera/sysinfo.h>

#include "vespera/time.h"
#include "vespera/mm/memory.h"
#include "vespera/realm/realm_ops.h"

namespace syscalls::internal {

    i64 sys_sysinfo(u64 arg0, u64, u64, u64, u64, u64) {
        auto info = reinterpret_cast<struct sysinfo *>(arg0);
        if (!info) return -EFAULT;

        sysinfo kinfo = {};

        kinfo.uptime = static_cast<i32>(kernel::time::get_uptime_ms() / 1000);
        kinfo.totalram = kernel::memory::get_total_ram();
        kinfo.freeram = kernel::memory::get_free_ram();
        kinfo.procs = kernel::realm::get_realm_count();
        kinfo.totalswap = 0;
        kinfo.freeswap = 0;
        kinfo.sharedram = 0;
        kinfo.bufferram = 0;
        kinfo.totalhigh = 0;
        kinfo.freehigh = 0;
        kinfo.mem_unit = 1;

        if (!copy_to_user(info, &kinfo, sizeof(struct sysinfo))) {
            return -EFAULT;
        }

        return SUCCESS_CODE;
    }

}  // namespace syscalls::internal