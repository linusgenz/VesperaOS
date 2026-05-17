/**
 * @file display_manager.h
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
#ifndef VESPERAOS_DISPLAY_MANAGER_H
#define VESPERAOS_DISPLAY_MANAGER_H
#include <vespera/sync/spinlock.h>

#include "vespera/graphics/IRenderDriver.h"

struct KernelDevice;
struct DisplayBackend {
    IRenderDriver* drv;
    KernelDevice* kd;
};

struct display_backend_hook {
    void (*fn)(DisplayBackend new_backend, void* ctx);
    void* ctx;
};

class DisplayManager {
   public:
    static void init(DisplayBackend initial);
    static void set_primary(DisplayBackend backend);
    static DisplayBackend primary();

    /**
     * @brief Registers a callback invoked whenever the primary display backend changes.
     *
     * Called with the new backend under the display lock. Callbacks must not call
     * back into DisplayManager (deadlock). Registered slots are never freed —
     * callers must ensure the ctx pointer outlives the DisplayManager.
     *
     * @return true if the hook was registered, false if MAX_HOOKS is exceeded.
     */
    static bool register_backend_hook(void (*fn)(DisplayBackend, void*), void* ctx);

   private:
    static constexpr usize MAX_HOOKS = 4;

    static inline DisplayBackend primary_{};
    static inline Spinlock lock_{};
    static inline display_backend_hook hooks_[MAX_HOOKS]{};
    static inline usize hook_count_{0};
};

#endif  // VESPERAOS_DISPLAY_MANAGER_H
