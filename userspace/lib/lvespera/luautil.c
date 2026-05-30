// luautil.c
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

#include "luautil.h"
#include "lauxlib.h"
#include <string.h>

int64_t lua_get_table_int(lua_State *L, const char *table, const char *key, int64_t default_val) {
    lua_getglobal(L, table);
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return default_val; }

    lua_getfield(L, -1, key);
    int64_t res = lua_isnumber(L, -1) ? lua_tointeger(L, -1) : default_val;
    lua_pop(L, 2);
    return res;
}

bool lua_get_table_string(lua_State *L, const char *table, const char *key, char *dest, size_t dest_len, const char *default_val) {
    lua_getglobal(L, table);
    if (!lua_istable(L, -1)) {
        if (default_val) strncpy(dest, default_val, dest_len - 1);
        lua_pop(L, 1);
        return false;
    }

    lua_getfield(L, -1, key);
    if (lua_isstring(L, -1)) {
        strncpy(dest, lua_tostring(L, -1), dest_len - 1);
        dest[dest_len - 1] = '\0';
        lua_pop(L, 2);
        return true;
    }

    if (default_val) {
        strncpy(dest, default_val, dest_len - 1);
        dest[dest_len - 1] = '\0';
    }
    lua_pop(L, 2);
    return false;
}

bool lua_get_table_bool(lua_State *L, const char *table, const char *key, bool default_val) {
    lua_getglobal(L, table);
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return default_val; }

    lua_getfield(L, -1, key);
    bool res = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : default_val;
    lua_pop(L, 2);
    return res;
}