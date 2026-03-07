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
#include <stdint.h>

#define PRIORITY_NONE 0
#define DEFAULT_UNIT_STACK_SIZE 0x20000

struct UnitConfig {
    const char *name = "unnamed_unit";
    uint8_t cpu_id = 0;
    uint8_t priority = 0;
    uint64_t stack_size = DEFAULT_UNIT_STACK_SIZE;
    HandleId *initial_handles = nullptr;
    uint64_t initial_handle_count = 0;
    bool is_idle = false;
    bool is_user = false;
    uint64_t user_stack_size = 0;
    bool auto_schedule = true;
    const char **argv;
    const char **envp;
};

#endif  // VESPERAOS_UNIT_CONFIG_H
