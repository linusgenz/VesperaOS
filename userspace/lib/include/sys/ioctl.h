// ioctl.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 22.09.25.
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

#ifndef VESPLIB_IOCTL_H
#define VESPLIB_IOCTL_H

#include <stdint.h>

#include <vespera/ioctl.h>

typedef uint64_t ioctl_request_t;

/**
 *
 * @param fd File descriptor of the device.
 * @param request Request code.
 * @param arg Pointer to request-specific argument.
 * @return 0 on success, negative error code on failure.
 */
int64_t ioctl(int fd, ioctl_request_t request, void* arg);

#endif //VESPLIB_IOCTL_H