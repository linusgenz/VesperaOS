// sys_munmap.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 27.09.25.
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

#include <errno.h>
#include <cstdint>
#include <kernel/memory.h>
#include <kernel/scheduling.h>

namespace syscalls::internal {
    int64_t sys_munmap(uint64_t addr, uint64_t length, uint64_t, uint64_t, uint64_t, uint64_t) {
        if (length == 0 || addr % PAGE_SIZE != 0) {
            return -EINVAL;
        }
        return 0;
// see sysstd reference on error codes for future impl.
/*
        // Page align
        length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        Unit* cur = kernel::scheduling::get_current_unit();
        if (!cur || !cur->is_user) return -EACCES;

        VmArea* prev = nullptr;
        VmArea* vma  = cur->vma_list;*/


    }

}