// sys_close.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 02.08.25.
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

#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <kernel/units/unit.h>

#include <vespera/types.h>
#include "../handle_resolution.h"
#include <kernel/realm/handle_table.h>

namespace syscalls::internal {
    i64 sys_close(u64 arg0, u64, u64, u64, u64, u64) {
        const HandleId hid = arg0;

        const auto rh = SYSCALL_TRY(
            resolve_handle(hid, /*type_mask=*/0, /*required_caps=*/0)
        );

        rh.realm->handle_table->release(hid);

        return SUCCESS_CODE;
    }
}  // namespace syscalls::internal
