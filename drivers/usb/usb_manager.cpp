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

#include "../../kernel/sync/spinlock.h"
#include "../../kernel/sync/completion.h"

completion_t USBManager::all_controllers_ready;
uint8_t USBManager::expected_controllers = 0;
uint8_t USBManager::initialized_controllers = 0;
spinlock_t USBManager::lock;

void USBManager::init() {
    all_controllers_ready.init();
    __atomic_store_n(&expected_controllers, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&initialized_controllers, 0, __ATOMIC_RELEASE);
    lock.init();
}

void USBManager::increment_expected_count() {
    __atomic_fetch_add(&expected_controllers, 1, __ATOMIC_ACQ_REL);
}

void USBManager::notify_controller_ready() {
    spinlock_guard guard(lock);
    initialized_controllers++;

    if (initialized_controllers >= expected_controllers) {
        all_controllers_ready.complete();
    }
}

bool USBManager::wait_for_all_controllers(uint64_t timeout_ms) {
    uint8_t expected = __atomic_load_n(&expected_controllers, __ATOMIC_ACQUIRE);

    if (expected == 0) return true;
    return all_controllers_ready.wait_timeout(timeout_ms);
}

uint8_t USBManager::get_initialized_count() {
    return __atomic_load_n(&initialized_controllers, __ATOMIC_ACQUIRE);
}

uint8_t USBManager::get_expected_count() {
    return __atomic_load_n(&expected_controllers, __ATOMIC_ACQUIRE);
}