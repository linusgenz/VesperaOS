// sys_setpgid.cpp
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

#include <vespera/jobctl/jobctl.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera_errno.h>

namespace syscalls::internal {
    i64 sys_setpgid(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const RealmId self_id = kernel::scheduling::get_current_realm_id();
        if (self_id == 0) return -ESRCH;

        const RealmId target_rid = arg0 ? arg0 : self_id;
        const RealmId desired_pgid = arg1 ? arg1 : target_rid;

        const auto result = kernel::jobctl::set_pgid(self_id, target_rid, desired_pgid);

        if (result.is_err()) return result.to_errno();

        return 0;
    }
}  // namespace syscalls::internal
