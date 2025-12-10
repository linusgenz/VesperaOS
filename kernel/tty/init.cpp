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

#include "tty_device.h"
#include "init.h"

#include "../../filesystem/devfs/devfs.h"

namespace kernel::tty {
    void initialize_ttys() {
        active_tty = &tty_instances[0];
        for (int i = 0; i < 6; i++) {
            tty_init(&tty_instances[i]);
            const char *name = DeviceManager::AllocUniqueDeviceName("tty");
            tty_devices[i] = new TTYDevice(name, &tty_instances[i]);
        }
    }
}
