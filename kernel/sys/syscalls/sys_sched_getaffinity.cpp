// sys_sched_getaffinity.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.08.26.
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
#include <vespera/types.h>
#include "../user_copy.h"
#include "cpu/cpu_manager.h"
#include "uapi/vespera/sched.h"

namespace syscalls::internal {

    i64 sys_sched_getaffinity(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        auto pid = static_cast<i32>(arg0);
        auto cpusetsize = static_cast<usize>(arg1);
        auto user_mask = reinterpret_cast<cpu_set_t*>(arg2);

        if (!user_mask) return -EFAULT;
        if (cpusetsize == 0) return -EINVAL;

        cpu_set_t kmask{};

        // no pinning currently supported, every realm can be executed on every cpu
        if (pid < 0) return -EINVAL;

        const u32 cpu_count = cpu_manager::get_online_cpu_count();
        for (u32 i = 0; i < cpu_count && i < CPU_SETSIZE; ++i) {
            kmask.__bits[__CPU_WORD(i)] |= __CPU_MASK(i);
        }

        const usize copy_bytes = (cpusetsize < sizeof(cpu_set_t)) ? cpusetsize : sizeof(cpu_set_t);

        if (!copy_to_user(user_mask, &kmask, copy_bytes)) {
            return -EFAULT;
        }

        return static_cast<i64>(copy_bytes);
    }

}  // namespace syscalls::internal