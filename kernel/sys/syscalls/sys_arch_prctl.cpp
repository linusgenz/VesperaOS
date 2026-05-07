// sys_arch_prctl.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 05.05.26.
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

#include <arch/x86_64/cpu/msr.h>
#include <vespera_errno.h>
#include <vespera/log.h>
#include <vespera/scheduling.h>

constexpr u64 ARCH_SET_GS = 0x1001;
constexpr u64 ARCH_SET_FS = 0x1002;
constexpr u64 ARCH_GET_FS = 0x1003;
constexpr u64 ARCH_GET_GS = 0x1004;

namespace syscalls::internal {

    i64 sys_arch_prctl(u64 code, u64 addr, u64, u64, u64, u64) {
        Unit* unit = kernel::scheduling::get_current_unit();
        if (!unit) return static_cast<u64>(-ESRCH);

        switch (code) {
            case ARCH_SET_FS:
                // Store in the unit so it survives context switches,
                // and apply immediately to the hardware MSR.
                unit->context.fs_base = addr;
                wrmsr(MSR_FS_BASE, addr);
                return 0;

            case ARCH_GET_FS: {
                // addr is a pointer to u64 in userspace — write the current fs_base into it.
                auto* out = reinterpret_cast<u64*>(addr);
                if (!out) return static_cast<u64>(-EINVAL);
                *out = unit->context.fs_base;
                return 0;
            }

            case ARCH_SET_GS:
                // GS is reserved for the kernel (per-CPU GsData).
                // Userspace must not set GS_BASE directly.
                return static_cast<u64>(-EPERM);

            case ARCH_GET_GS:
                return static_cast<u64>(-EPERM);

            default:
                return static_cast<u64>(-EINVAL);
        }
    }

}  // namespace syscalls::internal