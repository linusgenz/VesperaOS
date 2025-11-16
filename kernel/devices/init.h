// init.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 15.11.25.
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

#ifndef VESPERAOS_INIT_DEV_H
#define VESPERAOS_INIT_DEV_H

#include "../devices/misc/null.h"
#include "../devices/misc/zero.h"
#include "../devices/misc/cpuinfo.h"
#include "../devices/misc/full.h"
#include "../devices/misc/rtc.h"
#include "../devices/misc/uptime.h"
#include "../devices/misc/urandom.h"
#include "../devices/misc/version.h"
#include "../devices/log_device.h"

static ZeroDevice *zero_dev = nullptr;
static NullDevice *null_dev = nullptr;
static URandomDevice *urand_dev = nullptr;
static FullDevice *full_dev = nullptr;
static RTCDevice *rtc_dev = nullptr;
static UptimeDevice *uptime_dev = nullptr;
static VersionDevice *version_dev = nullptr;
static CPUInfoDevice *cpuinfo_dev = nullptr;
static LogDevice *log_dev = nullptr;

void initialize_devices();

#endif //VESPERAOS_INIT_DEV_H