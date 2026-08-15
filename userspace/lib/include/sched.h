// sched.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.08.26.
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
#ifndef _SCHED_H
#define _SCHED_H

#include <bits/sched.h>

void sched_yield(void);

/* Set the CPU affinity for a task */
// int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t* cpuset);

/* Get the CPU affinity for a task */
int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t* cpuset);

#endif //_SCHED_H
