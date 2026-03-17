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

#include "../../filesystem/devfs/devfs.h"
#include "tty_device.h"

namespace kernel::tty {
    void initialize_ttys() {
        keyboard_focus_tty = &tty_instances[0];
        auto term = kernel::SystemManager::get_system_terminal();
        for (int i = 0; i < 6; i++) {
            tty_init(&tty_instances[i], term);
            char name[16];
            DeviceManager::alloc_unique_device_name("tty", name, sizeof(name));
            tty_devices[i] = new TtyDevice(name, &tty_instances[i]);
        }
    }
}  // namespace kernel::tty
