// ioctl_devinfo.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 11.03.26.
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
#ifndef VESPERAOS_IOCTL_DEVINFO_H
#define VESPERAOS_IOCTL_DEVINFO_H

#include <vespera/ioctl.h>

typedef struct {
    char model[128];
    char serial[64];
    char vendor[64];
    char firmware[32];
} devinfo_t;

typedef struct {
    char value[128];
} devinfo_string_t;

#define IOCTL_DEVINFO_GET_ALL    IOR('D', 0x00, devinfo_t)
#define IOCTL_DEVINFO_GET_MODEL  IOR('D', 0x01, devinfo_string_t)
#define IOCTL_DEVINFO_GET_SERIAL IOR('D', 0x02, devinfo_string_t)
#define IOCTL_DEVINFO_GET_VENDOR IOR('D', 0x03, devinfo_string_t)
#define IOCTL_DEVINFO_GET_FW     IOR('D', 0x04, devinfo_string_t)

#endif  // VESPERAOS_IOCTL_DEVINFO_H
