// sys_chronos_checkpoint.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.06.26.
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

#define VESPERA_CHRONOS 1

#include <vespera/debug/chronos.h>
#include <vespera_errno.h>

#include "sys/user_copy.h"
#include "vespera/scheduling.h"

namespace syscalls::internal {
    i64 sys_chronos_checkpoint(u64 cp, u64, u64, u64, u64, u64) {
#if VESPERA_CHRONOS
        auto* user_ptr = reinterpret_cast<const chronos_user_checkpoint_t*>(cp);
        if (!user_ptr)
            return -EFAULT;

        ::chronos::UserCheckpoint kcp{};
        if (!syscalls::copy_from_user(&kcp, user_ptr, sizeof(kcp)))
            return -EFAULT;

        kcp.phase[::chronos::PHASE_MAX - 1] = '\0';
        kcp.label[::chronos::LABEL_MAX - 1] = '\0';

        u32 realm_id = static_cast<u32>(kernel::scheduling::get_current_realm_id());
        kernel::chronos::checkpoint_from_user(realm_id, kcp);
        return 0;
#else
        (void)user_ptr;
        return -ENOSYS;
#endif
    }

    i64 sys_chronos_summary(u64, u64, u64, u64, u64, u64) {
#if VESPERA_CHRONOS
        kernel::chronos::dump_dbc();
#else
        (void)user_ptr;
        return -ENOSYS;
#endif
    }
}
