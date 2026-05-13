// cpu_manager_impl.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 12.05.26.
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

#ifndef VESPERAOS_VESPERA_CPU_CPU_MANAGER_IMPL_H
#define VESPERAOS_VESPERA_CPU_CPU_MANAGER_IMPL_H

namespace cpu_manager {

    template<typename F>
    void for_each_cpu(F&& fn) {
        const u8 count = get_total_cpu_count();
        for (u32 i = 0; i < count; i++) {
            CpuInfo* info = get_cpu_info_by_index(i);
            if (info) fn(*info);
        }
    }

    template<typename F>
    void for_each_online_cpu(F&& fn) {
        for_each_cpu([&](CpuInfo& cpu) {
            if (cpu.state == CpuState::Online) fn(cpu);
        });
    }

} // namespace cpu_manager

#endif // VESPERAOS_VESPERA_CPU_CPU_MANAGER_IMPL_H