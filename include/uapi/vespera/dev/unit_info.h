/**
 * @file unit_info.h
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

#include <vespera/types.h>

/**
 *
 * @defgroup unit_state Unit State Constants
 * @brief Possible lifecycle states of a Unit.
 *
 * These constants describe the current execution state of a Unit and are
 * stored in the @ref unit_info::state field. The values are stable across
 * kernel versions and safe to use in userspace.
 *
 * @{
 */

/** @brief Unit has been created but not yet scheduled for the first time. */
#define UNIT_STATE_NEW 0

/** @brief Unit is ready to run and waiting to be picked up by the scheduler. */
#define UNIT_STATE_READY 1

/** @brief Unit is currently executing on a CPU. */
#define UNIT_STATE_RUNNING 2

/** @brief Unit is blocked, waiting for a resource, event, or I/O. */
#define UNIT_STATE_BLOCKED 3

/**
 * @brief Unit has finished execution but its exit code has not yet been collected.
 *
 * A Unit stays in this state until its Realm or a parent Unit reads
 * the exit code, after which it transitions to @ref UNIT_STATE_TERMINATED.
 */
#define UNIT_STATE_ZOMBIE 4

/** @brief Unit has fully terminated and all its resources have been released. */
#define UNIT_STATE_TERMINATED 5

/** @} */

/**
 * @brief Structure representing information about a Unit.
 *
 * This structure is used when querying the state of a Unit within a Realm.
 * A Unit is conceptually similar to a thread, running inside a Realm
 * (which acts like a process). It contains runtime and scheduling
 * information, resource handles, stack layout, and termination state.
 *
 * The @ref state field holds one of the @ref unit_state constants.
 */
typedef struct unit_info {
    u32 id;        ///< Unique identifier of the Unit
    u32 realm_id;  ///< Identifier of the Realm this Unit belongs to

    /**
     * @brief Current lifecycle state of the Unit.
     *
     * One of the UNIT_STATE_* constants (e.g. @ref UNIT_STATE_RUNNING).
     */
    u8 state;

    u8 priority;   ///< Scheduling priority of the Unit
    u8 cpu_id;     ///< ID of the CPU the Unit is currently running on
    i32 exit_code;  ///< Exit code if the Unit has terminated; 0 otherwise

    u64 handle_count;        ///< Number of handles/resources currently held by the Unit
    u64 kernel_stack_start;  ///< Start address of the Unit's kernel stack
    u64 kernel_stack_end;    ///< End address of the Unit's kernel stack
    u64 user_stack_start;    ///< Start address of the Unit's user stack
    u64 user_stack_end;      ///< End address of the Unit's user stack
} unit_info_t;

#endif  // VESPERAOS_UNITINFO_H