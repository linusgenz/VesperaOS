// ioctl_tty.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 18.03.26.
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
#ifndef VESPERAOS_IOCTL_TTY_H
#define VESPERAOS_IOCTL_TTY_H

#include <vespera/ioctl.h>

#define TTY_MODE_CANONICAL  0
#define TTY_MODE_RAW        1

typedef struct {
    int mode;        // TTY_MODE_*
    int echo;        // 1 = echo input, 0 = no echo
} tty_mode_t;

typedef struct {
    unsigned short rows;
    unsigned short cols;
} tty_size_t;

#define IOCTL_TTY_GET_MODE IOR('T', 0x01, tty_mode_t)
#define IOCTL_TTY_SET_MODE IOW('T', 0x02, tty_mode_t)
#define IOCTL_TTY_GET_SIZE IOR('T', 0x03, tty_size_t)

#endif  // VESPERAOS_IOCTL_TTY_H
