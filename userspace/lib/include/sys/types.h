// types.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 14.08.26.
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
#ifndef VESPLIB_TYPES_H
#define VESPLIB_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <vespera/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t FILE_HANDLE;
typedef uint64_t CHANNEL_HANDLE;
typedef uint64_t DIR_HANDLE;

#ifdef __cplusplus
}
#endif

#endif //VESPLIB_TYPES_H
