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
#include <stdint.h>
#include "../types.h"
#include "../capabilities.h"

/**
 * @brief Structure representing information about a Realm.
 *
 * This structure is used when querying the state of a Realm, via
 * RealmFS. It contains runtime and configuration
 * information about the Realm, including resource limits, capabilities,
 * scheduling, and working directory.
 */
typedef struct realm_info
{
    realm_id_t id; ///< Unique identifier of the Realm

    char name[64]; ///< Name of the Realm
    uint64_t memory_limit; ///< Maximum memory allowed for this Realm (bytes)
    uint64_t max_units; ///< Maximum number of Units this Realm may spawn
    uint64_t unit_count; ///< Current number of Units in this Realm
    uint8_t sched_priority; ///< Scheduling priority of the Realm
    uint64_t cpu_time_accumulated; ///< Total CPU time used by the Realm

    char cwd_path[256]; ///< Current working directory of the Realm
    capability_set_t capabilities; ///< Bitmask representing the Realm's capabilities
} realm_info_t;


#endif //VESPERAOS_REALM_INFO_H
