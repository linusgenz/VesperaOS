// unit.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 11.06.26.
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
#ifndef VESPLIB_UNIT_H
#define VESPLIB_UNIT_H

#include <stdint.h>

typedef uint64_t UnitID;
typedef uint64_t RealmID;

/**
 * @brief Spawn a new unit (thread) inside an existing realm.
 *
 * @param realm_id The ID of the target realm.
 * @param entry_point Pointer to the function or code where the unit should start.
 * @param arg_ptr Pointer to argument data for the unit.
 * @return Unit ID on success, negative error code on failure.
 */
UnitID spawn_unit(RealmID realm_id, uint64_t entry_point, uint64_t arg_ptr, uint64_t stack_size);

/**
 * @brief Wait for a unit within the current realm to finish.
 *
 * Blocks until the unit with the given ID has terminated.
 *
 * @param unit_id The ID of the unit to wait for.
 * @param exit_code_out Pointer to store the unit's exit/return code, or NULL if unused.
 * @return 0 on success, negative error code on failure (e.g. -ESRCH if unit_id invalid/already reaped).
 */
int64_t join_unit(UnitID unit_id, int64_t* exit_code_out);


UnitID get_unit_id(void);

#endif //VESPLIB_UNIT_H
