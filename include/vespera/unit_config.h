// unit_config.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 07.03.26.
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
#ifndef VESPERAOS_UNIT_CONFIG_H
#define VESPERAOS_UNIT_CONFIG_H
#include <vespera/types.h>

#define PRIORITY_NONE 0
constexpr size_t DEFAULT_UNIT_STACK_SIZE = 8 * 1024 * 1024; // 8MB
static constexpr usize TLS_REGION_SIZE = 4 * 1024 * 1024;

/**
 * @brief Parameters describing the loaded ELF image, needed to build the process's aux vector.
 *
 * Populated from @ref ElfLoader::LoadResult by the exec/spawn path. When @p has_interp is
 * false, @p interp_base is ignored and `AT_BASE` is written as 0 downstream, per the SysV
 * convention for statically-linked or non-PT_INTERP images.
 */
struct AuxVectorInfo {
    uptr phdr_vaddr;
    u16 phent;
    u16 phnum;
    uptr entry_point;
    bool has_interp;
    uptr interp_base;
};

struct UnitConfig {
    const char* name = "unnamed_unit";
    u8 cpu_id = 0;
    u8 priority = 0;
    u64 stack_size = DEFAULT_UNIT_STACK_SIZE;
    HandleId* initial_handles = nullptr;
    u64 initial_handle_count = 0;
    bool is_idle = false;
    bool is_user = false;
    bool is_main_unit = false;
    u64 user_stack_size = 0;
    bool auto_schedule = true;
    const char** argv = nullptr;
    const char** envp = nullptr;

    bool has_aux_info = false;

    /** @brief ELF aux vector parameters. Only read when @ref has_aux_info is true. */
    AuxVectorInfo aux_info{};
};

#endif  // VESPERAOS_UNIT_CONFIG_H
