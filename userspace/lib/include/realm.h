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

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Spawn a new realm (isolated execution context).
 *
 * The new realm starts with a single initial unit.
 *
 * @param path_ptr Pointer to the path of the executable binary.
 * @param argc Number of arguments.
 * @param argv_ptr Pointer to an array of argument strings.
 * @return Realm ID on success, negative error code on failure.
 */
int64_t spawn_realm(const char* path_ptr, uint32_t argc, const char** argv);

/**
 * @brief Spawn a new unit (thread) inside an existing realm.
 *
 * @param realm_id The ID of the target realm.
 * @param entry_point Pointer to the function or code where the unit should start.
 * @param arg_ptr Pointer to argument data for the unit.
 * @return Unit ID on success, negative error code on failure.
 */
int64_t spawn_unit(uint64_t realm_id, uint64_t entry_point, uint64_t arg_ptr);

/**
 * @brief Terminate the current unit.
 *
 * This will stop the unit and remove it from its realm.
 *
 * @param code Exit code for the unit.
 * @return Does not return; halts the unit.
 */
__attribute__((noreturn))
void exit(uint64_t code);

/**
 * @brief Terminate an entire realm and all its units.
 *
 * @param realm_id The ID of the realm to terminate.
 * @param code Exit code for the realm.
 * @return 0 on success, negative error code on failure.
 */
int64_t exit_realm(uint64_t realm_id, uint64_t code);


#endif //VESPERAOS_REALM_H