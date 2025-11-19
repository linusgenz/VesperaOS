// meminfo.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 17.11.25.
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

#ifndef VESPERAOS_MEMINFO_H
#define VESPERAOS_MEMINFO_H

#include <stdint.h>

/**
 * @brief Memory information provided by the meminfo device.
 *
 * This struct mirrors the binary data returned by the kernel meminfo device.
 * All fields represent raw byte counts and are guaranteed to be consistent
 * at the moment of the read call.
 */
typedef struct {
    uint64_t total_ram;    ///< Total RAM in bytes
    uint64_t used_ram;     ///< Used RAM in bytes
    uint64_t free_ram;     ///< Free RAM in bytes
    uint64_t reserved_ram; ///< Reserved RAM in bytes by the System
} meminfo_t;

#endif // VESPERAOS_MEMINFO_H
