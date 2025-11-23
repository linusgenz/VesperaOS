// deadlock_detector.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 20.11.25.
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

#include <log.h>

#include "deadlock_detector.h"

#include <kerrno.h>

#include "lock_debug.h"
#include "../system/system_manager.h"
#include "../utils/panic.h"

static bool enabled = false;

void deadlock_detector_init() {
    enabled = true;
}

void deadlock_detector_tick() {
    if (!enabled) return;
    bool found = lock_debug_detect_deadlocks_and_report();
    if (found) {
        Log::PrintLn("Deadlock(s) detected. See dump above.");
        kernel::SystemManager::system_panic("DEADLOCK DETECTED", -EDEADLK);
    }
}