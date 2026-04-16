// sys_sleep.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 13.08.25.
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

#include <vespera/scheduling.h>
#include <vespera/time.h>

namespace syscalls::internal {
    i64 sys_sleep(u64 arg0, u64, u64, u64, u64, u64) {
        Unit* current = kernel::scheduling::get_current_unit();

        const virt_addr_t saved_rsp = current->context.stack_pointer;
        kernel::time::sleep_ms(10);
        current->context.stack_pointer = saved_rsp;

        return SUCCESS_CODE;
    }
}  // namespace syscalls::internal
