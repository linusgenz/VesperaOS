// mkdir.cpp
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

#include "../../../filesystem/vfs/vfs.h"
#include "../../../include/errno.h"

namespace syscalls::internal {
    int64_t sys_mkdir(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
        const auto path = reinterpret_cast<const char*>(arg0);
        if (!path) return -EINVAL;

        const int result = VFS::mkdir(path);
        return result < 0 ? -result : 0; // Negative result already uses errno codes
    }

}