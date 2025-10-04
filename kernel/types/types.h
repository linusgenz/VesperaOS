// types.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 19.09.25.
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

#ifndef VESPERAOS_TYPES_H
#define VESPERAOS_TYPES_H

#define MAX_UNIT_HANDLE_SLOTS 64
#define MAX_HANDLES_PER_REALM 4096

#define KERNEL_REALM_SYSTEM 1
#define KERNEL_REALM_DRIVER 2
#include <cstdint>

typedef long int ssize_t;

typedef uint64_t HandleID;
typedef uint64_t UnitID;
typedef uint64_t RealmID;

// Handle typs (upper 16 bits)
#define HANDLE_TYPE_MASK    0xFFFF000000000000ULL
#define HANDLE_ID_MASK      0x0000FFFFFFFFFFFFULL


#define HANDLE_TYPE_TTY 0x1000000000000000ULL
#define HANDLE_TYPE_FILE    0x2000000000000000ULL
#define HANDLE_TYPE_DIRECTORY 0x3000000000000000ULL
#define HANDLE_TYPE_CHANNEL 0x4000000000000000ULL
#define HANDLE_TYPE_UNIT    0x5000000000000000ULL
#define HANDLE_TYPE_REALM   0x6000000000000000ULL
#define HANDLE_TYPE_DEVICE  0x7000000000000000ULL

#define HANDLE_STDIN   (HANDLE_TYPE_TTY | 0x0000000000000000ULL)
#define HANDLE_STDOUT  (HANDLE_TYPE_TTY | 0x0000000000000001ULL)
#define HANDLE_STDERR  (HANDLE_TYPE_TTY | 0x0000000000000002ULL)

// Capabilities
typedef uint64_t CapabilitySet;
#define CAP_NONE          0x0000000000000000ULL
#define CAP_READ          0x0000000000000001ULL
#define CAP_WRITE         0x0000000000000002ULL
#define CAP_RW            (CAP_READ | CAP_WRITE)
#define CAP_EXECUTE       0x0000000000000004ULL
#define CAP_NETWORK_BIND  0x0000000000000020ULL
#define CAP_UNIT_SPAWN    0x0000000000000100ULL
#define CAP_DEVICE_ACCESS 0x0000000000000200ULL
#define CAP_ALL           0xFFFFFFFFFFFFFFFFULL

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040

// Seek-Konstanten
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

typedef enum {
    MOD_SUCCESS = 0,
    MOD_ERR_INVALID_HANDLE,
    MOD_ERR_PERMISSION_DENIED,
    MOD_ERR_OUT_OF_MEMORY,
    MOD_ERR_UNIT_NOT_FOUND,
    MOD_ERR_INVALID_OPERATION,
} ErrorCode;

#define PRIORITY_NONE 0

typedef struct {
    const char *name;
    uint8_t cpu_id;
    uint8_t priority;
    uint64_t stack_size;
    HandleID *initial_handles;
    uint64_t initial_handle_count;
    bool is_idle;
    bool is_user;
    uint64_t user_stack_size;
} UnitConfig;

typedef struct {
    const char *name;
    uint64_t memory_limit;
    CapabilitySet capabilities;
    uint64_t max_units;
    const char **envp;
} RealmConfig;


#endif //VESPERAOS_TYPES_H
