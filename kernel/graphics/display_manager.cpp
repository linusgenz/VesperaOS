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

bool DisplayManager::register_backend_hook(void (*fn)(const DisplayInfo&, void*), void* ctx, HookFlags flags) {
    DisplayInfo info{};
    const bool fire_immediately =
        (static_cast<u32>(flags) & static_cast<u32>(HookFlags::FIRE_IMMEDIATELY)) != 0;

    {
        SpinlockGuard guard(lock_);

        if (hook_count_ >= MAX_HOOKS)
            return false;

        hooks_[hook_count_++] = {fn, ctx};

        if (fire_immediately) {
            info = {
                .width = primary_.drv->screen_width_px(),
                .height = primary_.drv->screen_height_px(),
                .backend = primary_,
            };
        }
    }

    if (fire_immediately)
        fn(info, ctx);

    return true;
}

void DisplayManager::set_primary(const DisplayBackend backend) {
    SpinlockGuard guard(lock_);
    primary_ = backend;

    const DisplayInfo info{
        .width = backend.drv->screen_width_px(),
        .height = backend.drv->screen_height_px(),
        .backend = backend,
    };

    for (usize i = 0; i < hook_count_; i++) {
        hooks_[i].fn(info, hooks_[i].ctx);
    }
}

DisplayBackend DisplayManager::primary() {
    SpinlockGuard guard(lock_);
    return primary_;
}
