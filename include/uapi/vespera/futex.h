// futex.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 23.06.26.
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
#ifndef VESPERAOS_UAPI_FUTEX_H
#define VESPERAOS_UAPI_FUTEX_H

#define FUTEX_WAIT        0   // Wait until *uaddr != val
#define FUTEX_WAKE        1   // Wake up to val threads
#define FUTEX_WAKE_ALL    2   // Wake up all waiting threads

// Timeout flag: if set, `timeout` is an absolute CLOCK_MONOTONIC time
#define FUTEX_ABSTIME     (1 << 8)

#endif //VESPERAOS_UAPI_FUTEX_H
