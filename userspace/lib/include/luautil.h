// luautil.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 29.05.26.
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
#ifndef _LUAUTIL_H
#define _LUAUTIL_H

#include "lua.h"
#include <stdbool.h>
#include <stdint.h>

// Get an integer from a global table
int64_t lua_get_table_int(lua_State *L, const char *table, const char *key, int64_t default_val);

// Get a string from a global table and copy it safely into 'dest'
bool lua_get_table_string(lua_State *L, const char *table, const char *key, char *dest, size_t dest_len, const char *default_val);

// Get a Boolean from a global table
bool lua_get_table_bool(lua_State *L, const char *table, const char *key, bool default_val);

#endif  // VESPERAOS_LUAUTIL_H
