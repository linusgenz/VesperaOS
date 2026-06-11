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
UnitID spawn_unit(RealmID realm_id, uint64_t entry_point, uint64_t arg_ptr);

/* =========================
   MUTEX
   ========================= */

typedef struct ves_mutex {
   // 0 = unlocked
   // 1 = locked, no contention
   // 2 = locked, contention (futex waiters)
   int _state;
} ves_mutex_t;

/* lifecycle */
ves_mutex_t* ves_mutex_create(void);
void ves_mutex_destroy(ves_mutex_t* mtx);

/* operations */
void ves_mutex_lock(ves_mutex_t* mtx);
void ves_mutex_unlock(ves_mutex_t* mtx);
int ves_mutex_trylock(ves_mutex_t* mtx);

void ves_mutex_init(ves_mutex_t* mtx);

#endif //VESPLIB_UNIT_H
