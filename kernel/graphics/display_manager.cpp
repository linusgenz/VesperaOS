/**
 * @file display_manager.cpp
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 30.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
 */

#include <vespera/graphics/display_manager.h>

void DisplayManager::init(const DisplayBackend initial) {
    lock_.init("display_manager_lock");
    SpinlockGuard guard(lock_);
    primary_ = initial;
}

bool DisplayManager::register_backend_hook(void (*fn)(DisplayBackend, void*), void* ctx) {
    SpinlockGuard guard(lock_);
    if (hook_count_ >= MAX_HOOKS) return false;
    hooks_[hook_count_++] = { fn, ctx };
    return true;
}

void DisplayManager::set_primary(const DisplayBackend backend) {
    SpinlockGuard guard(lock_);
    primary_ = backend;

    for (usize i = 0; i < hook_count_; i++) {
        hooks_[i].fn(backend, hooks_[i].ctx);
    }
}

DisplayBackend DisplayManager::primary() {
    SpinlockGuard guard(lock_);
    return primary_;
}