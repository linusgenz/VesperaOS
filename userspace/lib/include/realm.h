// realm.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 22.09.25.
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

#ifndef VESPERAOS_REALM_H
#define VESPERAOS_REALM_H

#include <stddef.h>
#include <stdint.h>
#include <vespera/spawn.h>

typedef uint64_t RealmID;  ///< type representing a realm identifier
typedef uint64_t UnitID;   ///< type representing a unit (thread) identifier

/**
 * @brief Spawn a new realm (isolated execution context).
 *
 * The new realm starts with a single initial unit.
 *
 * @param path_ptr Pointer to the path of the executable binary.
 * @param argv Pointer to an array of argument strings.
 * @param envp Pointer to a NULL-terminated array of strings representing the environment variables for the new realm.
 * Can be @c NULL if no environment variables are needed.
 * @return Realm ID on success, negative error code on failure.
 */
RealmID spawn_realm(const char* path_ptr, char* const argv[], char* const envp[], spawn_config_t* cfg);

/**
 * @brief Spawn a new unit (thread) inside an existing realm.
 *
 * @param realm_id The ID of the target realm.
 * @param entry_point Pointer to the function or code where the unit should start.
 * @param arg_ptr Pointer to argument data for the unit.
 * @return Unit ID on success, negative error code on failure.
 */
UnitID spawn_unit(RealmID realm_id, uint64_t entry_point, uint64_t arg_ptr);

/**
 * @brief Terminate an entire realm and all its units.
 *
 * @param realm_id The ID of the realm to terminate.
 * @param code Exit code for the realm.
 * @return 0 on success, negative error code on failure.
 */
int64_t exit_realm(RealmID realm_id, uint64_t code);

/**
 * @brief Wait for a realm to finish execution.
 *
 * Blocks the current process until the realm with ID @p realm_id has completed.
 *
 * @param realm_id The ID of the realm to wait for.
 * @param status Pointer to an int to store the exit status, or @c NULL if unused.
 * @return 0 on success, or a negative error code on failure (e.g., -ECHILD).
 */
int wait_realm(RealmID realm_id, int* status);

#endif  // VESPERAOS_REALM_H
