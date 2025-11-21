// usb_manager.cpp
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

#include "usb_manager.h"

#include <cstdint>

#include "../../kernel/sync/atomic.h"
#include "../../kernel/sync/spinlock.h"
#include "../../kernel/sync/completion.h"

completion_t USBManager::all_controllers_ready;
atomic_u8 USBManager::expected_controllers;
atomic_u8 USBManager::initialized_controllers;
spinlock_t USBManager::lock;

void USBManager::init() {
    all_controllers_ready.init();
    expected_controllers.init();
    initialized_controllers.init();
    lock.init();
    lock_debug_register(&lock, "usb_manager_lock");
}

void USBManager::increment_expected_count() {
    expected_controllers.fetch_add(1);
}

void USBManager::notify_controller_ready() {
    spinlock_guard guard(lock);
    ++initialized_controllers;

    if (initialized_controllers.load() >= expected_controllers.load()) {
        all_controllers_ready.complete();
    }
}

bool USBManager::wait_for_all_controllers(uint64_t timeout_ms) {
    uint8_t expected = expected_controllers.load();

    if (expected == 0) return true;
    return all_controllers_ready.wait_timeout(timeout_ms);
}

uint8_t USBManager::get_initialized_count() {
    return initialized_controllers.load();
}

uint8_t USBManager::get_expected_count() {
    return expected_controllers.load();
}