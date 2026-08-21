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
#ifndef _FUTEX_H
#define _FUTEX_H

#include <stdint.h>
#include <sysstd.h>
#include <time.h>
#include <vespera/futex.h>

int futex_wait(const uint32_t* addr, uint32_t expected, const struct timespec* timeout);
int futex_wake(uint32_t* addr, uint32_t n);
int futex_wake_all(uint32_t* addr);
int futex_wait_until(const uint32_t* addr, uint32_t expected, const struct timespec* deadline);

#endif //VESPERAOS_FUTEX_H
