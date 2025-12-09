/**
 * @file unit_status.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 08.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef VESPERAOS_UNIT_STATUS_H
#define VESPERAOS_UNIT_STATUS_H
#include <cstdint>

typedef struct {
    uint32_t id;
    uint32_t realm_id;
    uint8_t state;
    uint8_t priority;
    uint8_t cpu_id;
    int32_t exit_code;
    uint64_t handle_count;
    uint64_t kernel_stack_start;
    uint64_t kernel_stack_end;
    uint64_t user_stack_start;
    uint64_t user_stack_end;
} unit_info_t;

#endif //VESPERAOS_UNIT_STATUS_H