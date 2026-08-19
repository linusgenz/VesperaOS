// unit.c
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

#include "unit.h"
#include "stdlib.h"

UnitID spawn_unit(RealmID realm_id, uint64_t entry_point, uint64_t arg_ptr, uint64_t stack_size) {
    return sys_unit_spawn(realm_id, entry_point, arg_ptr, stack_size, 0, 0);
}

int64_t join_unit(UnitID unit_id, int* exit_code_out) {
    return sys_join_unit(unit_id, (uint64_t)exit_code_out, 0, 0, 0, 0);
}

UnitID get_unit_id(void) {
    return sys_get_unid(0,0,0,0,0,0);
}