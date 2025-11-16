// init.cpp
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

#include "init.h"

void initialize_devices() {
    Channel *kernel_log_channel = Channel::create(32 * 1024);

    zero_dev = new ZeroDevice("zero");
    null_dev = new NullDevice("null");
    urand_dev = new URandomDevice("urandom");
    full_dev = new FullDevice("full");
    rtc_dev = new RTCDevice("rtc");
    uptime_dev = new UptimeDevice("uptime");
    version_dev = new VersionDevice("version");
    cpuinfo_dev = new CPUInfoDevice("cpuinfo");
    log_dev = new LogDevice(kernel_log_channel);
}