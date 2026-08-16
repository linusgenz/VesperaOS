// sys_sigprocmask.cpp
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

#include <vespera/types.h>
#include <vespera/scheduling.h>
#include <vespera/signals.h>
#include <vespera_errno.h>

#include "../user_copy.h"

namespace syscalls::internal {
    i64 sys_sigprocmask(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const auto how = static_cast<int>(arg0);
        const auto user_set = reinterpret_cast<const u64*>(arg1);
        const auto user_oldset = reinterpret_cast<u64*>(arg2);

        u64 kernel_new_set = 0;
        u64 kernel_old_set = 0;
        u64* new_set_ptr = nullptr;

        if (user_set) {
            if (!copy_from_user(&kernel_new_set, user_set, sizeof(u64))) {
                return -EFAULT;
            }
            new_set_ptr = &kernel_new_set;
        }

        Unit* current = kernel::scheduling::get_current_unit();

        i64 result = signal_update_mask(current, how, new_set_ptr, &kernel_old_set);

        if (result == 0 && user_oldset) {
            if (!copy_to_user(user_oldset, &kernel_old_set, sizeof(u64))) {
                return -EFAULT;
            }
        }

        return result;
    }
} // namespace syscalls::internal
