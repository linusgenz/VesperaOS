/**
 * @file unitinfo.h
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
#ifndef VESPERAOS_UNITINFO_H
#define VESPERAOS_UNITINFO_H

#include <stdint.h>

/**
 * @brief Structure representing information about a Unit.
 *
 * This structure is used when querying the state of a Unit within a Realm.
 * A Unit is conceptually similar to a thread, running inside a Realm
 * (which acts like a process). It contains runtime and scheduling
 * information, resource handles, stack layout, and termination state.
 */
typedef struct unit_info {
    uint32_t id; ///< Unique identifier of the Unit
    uint32_t realm_id; ///< Identifier of the Realm this Unit belongs to
    uint8_t state; ///< Current state of the Unit (e.g., running, blocked, terminated)
    uint8_t priority; ///< Scheduling priority of the Unit
    uint8_t cpu_id; ///< ID of the CPU the Unit is currently running on
    int32_t exit_code; ///< Exit code if the Unit has terminated

    uint64_t handle_count; ///< Number of handles/resources currently held by the Unit
    uint64_t kernel_stack_start; ///< Start address of the Unit's kernel stack
    uint64_t kernel_stack_end; ///< End address of the Unit's kernel stack
    uint64_t user_stack_start; ///< Start address of the Unit's user stack
    uint64_t user_stack_end; ///< End address of the Unit's user stack
} unit_info_t;


#endif //VESPERAOS_UNITINFO_H