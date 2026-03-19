// poll.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 19.03.26.
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
#ifndef VESPERAOS_POLL_H
#define VESPERAOS_POLL_H
#include <vespera/types.h>

#define POLLIN 0x01
#define POLLOUT 0x02
#define POLLERR 0x04
#define POLLHUP 0x08

typedef struct pollhdl {
    i64 hdl;
    i16 events; // POLLIN | POLLOUT
    i16 revents;
    i32 _pad;
} pollhdl_t;

#endif  // VESPERAOS_POLL_H
