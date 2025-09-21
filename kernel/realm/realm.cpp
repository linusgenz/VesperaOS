// realm.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 21.09.25.
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

#include "realm.h"
#include <log.h>
/*
handle_entry_t* Realm::lookup_handle(HandleID hid) {
    Log::PrintLn("Looking up handle: 0x%llx\n", hid);

    uint64_t raw = (uint64_t)(hid & HANDLE_ID_MASK);
    Log::PrintLn("Raw slot: %llu\n", raw);

    if (raw >= MAX_HANDLES_PER_REALM) {
        Log::PrintLn("Slot too large: %llu >= %llu\n", raw, MAX_HANDLES_PER_REALM);
        return nullptr;
    }

    if (!test_bit(raw)) {
        Log::PrintLn("Bit not set for slot %llu\n", raw);
        return nullptr;
    }

    handle_entry_t &he = handle_table.entries[raw];
    Log::PrintLn("Found entry with hid: 0x%llx, expected: 0x%llx\n", he.hid, hid);

    if (he.hid != hid) {
        Log::PrintLn("Handle ID mismatch!\n");
        return nullptr;
    }
    return &he;
}*/