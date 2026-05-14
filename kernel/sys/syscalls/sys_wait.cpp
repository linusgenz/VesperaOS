// sys_wait.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 23.09.25.
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

#include <vespera/realm/exit_code_table.h>
#include <vespera/realm/realm_ops.h>

namespace syscalls::internal {
    i64 sys_wait(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const RealmId child_rid = arg0;

        auto* status = reinterpret_cast<int*>(arg1);
        if (!status)
            return -EINVAL;

        const auto result = kernel::realm::wait(child_rid);

        if (result.is_err())
            return result.to_errno();

        *status = result.unwrap();

        return 0;
    }
}  // namespace syscalls::internal
