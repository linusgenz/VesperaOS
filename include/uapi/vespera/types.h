// types.h
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

#ifndef VESPERA_UAPI_TYPES_H
#define VESPERA_UAPI_TYPES_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef signed char        i8;
typedef signed short       i16;
typedef signed int         i32;
typedef signed long long   i64;

typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef uint64_t dev_t;      ///< Device IDs (major/minor combined)
typedef uint64_t ino_t;      ///< Inode numbers
typedef uint32_t mode_t;     ///< File modes and permission bits
typedef uint32_t nlink_t;    ///< Number of hard links
typedef uint32_t uid_t;      ///< User ID
typedef uint32_t gid_t;      ///< Group ID
typedef int64_t  off_t;      ///< File offsets / sizes (signed)
typedef int64_t  blksize_t;  ///< Block size for I/O
typedef int64_t  blkcnt_t;   ///< Number of allocated 512-byte blocks
typedef int64_t  time_t;     ///< Seconds since the epoch
typedef int32_t  pid_t;      ///< Process ID
typedef long ssize_t;    ///< Signed size (for read/write returns)

#endif