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

#include <vespera/types.h>
#include <vespera/sync/atomic.h>
#include <vespera/sync/completion.h>
#include <vespera/sync/spinlock.h>

Completion UsbManager::all_controllers_ready_;
AtomicU8 UsbManager::expected_controllers_;
AtomicU8 UsbManager::initialized_controllers_;
Spinlock UsbManager::lock_;

void UsbManager::init() {
    all_controllers_ready_.init();
    expected_controllers_.init();
    initialized_controllers_.init();
    lock_.init("usb_manager_lock");
}

void UsbManager::increment_expected_count() {
    expected_controllers_.fetch_add(1);
}

void UsbManager::notify_controller_ready() {
    SpinlockGuard guard(lock_);
    ++initialized_controllers_;

    if (initialized_controllers_.load() >= expected_controllers_.load()) {
        all_controllers_ready_.complete();
    }
}

bool UsbManager::wait_for_all_controllers(const u64 timeout_ms) {
    if (const u8 expected = expected_controllers_.load(); expected == 0) return true;
    return all_controllers_ready_.wait_timeout(timeout_ms);
}

u8 UsbManager::get_initialized_count() {
    return initialized_controllers_.load();
}

u8 UsbManager::get_expected_count() {
    return expected_controllers_.load();
}