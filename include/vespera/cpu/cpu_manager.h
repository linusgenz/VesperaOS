// cpu_manager.h
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
#ifndef VESPERAOS_VESPERA_CPU_CPU_MANAGER_H
#define VESPERAOS_VESPERA_CPU_CPU_MANAGER_H

#include <vespera/types.h>

namespace cpu_manager {

    enum class CpuState : u8 {
        Offline = 0,
        Starting = 1,
        Online = 2,
        Halted = 3,
    };

    struct CpuAccounting {
        u64 last_tick_tsc;
        u64 total_cycles;
        u64 idle_cycles;
    };

    /**
     * @brief Per-CPU runtime descriptor populated during SMP init.
     */
    struct CpuInfo {
        u32 apic_id;
        u32 cpu_id;
        CpuState state;
        uptr kernel_stack;
        uptr kernel_stack_top;
        CpuAccounting accounting;
        u32 current_task_id;
        bool is_bsp;
    };

    void initialize();
    void smp_init();
    void init_core(const CpuInfo* cpu);

    [[nodiscard]] u8 get_current_cpu_id();
    [[nodiscard]] u8 get_online_cpu_count();
    [[nodiscard]] u8 get_total_cpu_count();
    [[nodiscard]] CpuInfo* get_cpu_info(u32 apic_id);
    [[nodiscard]] CpuInfo* get_cpu_info_by_index(u32 index);

    void halt_cpu(u32 apic_id);
    void accounting_tick(u32 cpu_id, bool is_idle);
    [[nodiscard]] int get_cpu_usage_percent(u32 cpu_id);

    /**
     * @brief Calls @p fn for every CPU slot, regardless of state.
     *
     * @tparam F  Callable with signature `void(CpuInfo&)`.
     * @note  Safe to call from interrupt context.
     */
    template <typename F>
    void for_each_cpu(F&& fn);

    /**
     * @brief Calls @p fn only for CPUs in CpuState::Online.
     */
    template <typename F>
    void for_each_online_cpu(F&& fn);

}  // namespace cpu_manager

#include <vespera/cpu/cpu_manager_impl.h>

#endif  // VESPERAOS_VESPERA_CPU_CPU_MANAGER_H