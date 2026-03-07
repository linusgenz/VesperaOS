// usb_manager.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 05.10.25.
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

#ifndef VESPERAOS_USB_MANAGER_H
#define VESPERAOS_USB_MANAGER_H

#include <stdint.h>
#include <vespera/sync/atomic.h>
#include <vespera/sync/completion.h>
#include <vespera/sync/spinlock.h>

class UsbManager {
private:
    static Completion all_controllers_ready_;
    static AtomicU8 expected_controllers_;
    static AtomicU8 initialized_controllers_;
    static Spinlock lock_;
public:
    static void init();
    static void notify_controller_ready();
    static bool wait_for_all_controllers(uint64_t timeout_ms = 10000);
    static uint8_t get_initialized_count();
    static uint8_t get_expected_count();
    static void increment_expected_count();
};

#endif //VESPERAOS_USB_MANAGER_H