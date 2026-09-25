// timerfd.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 25.09.26.
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
#ifndef VESPERAOS_UAPI_TIMERFD_H
#define VESPERAOS_UAPI_TIMERFD_H

#include "time.h"

struct itimerspec
{
    struct timespec it_interval;
    struct timespec it_value;
};

enum
{
    TFD_TIMER_ABSTIME = 1 << 0,
#define TFD_TIMER_ABSTIME TFD_TIMER_ABSTIME
    TFD_TIMER_CANCEL_ON_SET = 1 << 1
#define TFD_TIMER_CANCEL_ON_SET TFD_TIMER_CANCEL_ON_SET
  };

enum
{
    TFD_CLOEXEC = 02000000,
#define TFD_CLOEXEC TFD_CLOEXEC
    TFD_NONBLOCK = 00004000
#define TFD_NONBLOCK TFD_NONBLOCK
  };


#endif //VESPERAOS_UAPI_TIMERFD_H
