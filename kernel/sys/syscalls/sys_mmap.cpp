// sys_mmap.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 26.09.25.
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

#include <vespera/mm/vm.h>
#include <vespera/scheduling.h>

namespace syscalls::internal {

    i64 sys_mmap(u64 addr, u64 length, u64 prot, u64 flags, u64 handle, u64 offset) {
        return kernel::vm::mmap(
            kernel::scheduling::get_current_unit(),
            static_cast<uptr>(addr), static_cast<usize>(length),
            prot, flags, handle, offset
        );
    }

} // namespace syscalls::internal