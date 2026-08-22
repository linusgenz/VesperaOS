// handles.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 07.03.26.
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
#ifndef VESPERAOS_HANDLES_H
#define VESPERAOS_HANDLES_H

#define INVALID_HANDLE ((FILE_HANDLE) - 1ULL)

// Handle typs (upper 16 bits)
#define HANDLE_TYPE_MASK 0xFFFF000000000000ULL
#define HANDLE_ID_MASK 0x0000FFFFFFFFFFFFULL

/* Handle type definitions */
#define HANDLE_TYPE_FILE 0x2000000000000000ULL
#define HANDLE_TYPE_DIRECTORY 0x3000000000000000ULL
#define HANDLE_TYPE_CHANNEL 0x4000000000000000ULL
#define HANDLE_TYPE_UNIT 0x5000000000000000ULL
#define HANDLE_TYPE_REALM 0x6000000000000000ULL
#define HANDLE_TYPE_DEVICE 0x7000000000000000ULL
#define HANDLE_TYPE_PIPE 0x8000000000000000ULL
#define HANDLE_TYPE_SHM 0x9000000000000000ULL
#define HANDLE_TYPE_FIFO 0x1100000000000000ULL


/* Standard stream handles */
#define HANDLE_STDIN   (HANDLE_TYPE_DEVICE | 0x0000000000000000ULL)
#define HANDLE_STDOUT  (HANDLE_TYPE_DEVICE | 0x0000000000000001ULL)
#define HANDLE_STDERR  (HANDLE_TYPE_DEVICE | 0x0000000000000002ULL)

#define HANDLE_VBUS    (HANDLE_TYPE_CHANNEL | 0x0000000000000003ULL)
#endif  // VESPERAOS_HANDLES_H
