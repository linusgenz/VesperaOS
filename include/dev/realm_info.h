/**
 * @file realm_info.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 09.12.25.
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
#ifndef VESPERAOS_REALM_INFO_H
#define VESPERAOS_REALM_INFO_H
#include <cstdint>
#include "../../kernel/types/types.h"

struct realm_info_t {
    RealmID id;
    char name[64];
    uint64_t memory_limit;
    uint64_t max_units;
    uint64_t unit_count;
    uint8_t sched_priority;
    uint64_t cpu_time_accumulated;

    char cwd_path[256];
    CapabilitySet capabilities;
};


#endif //VESPERAOS_REALM_INFO_H