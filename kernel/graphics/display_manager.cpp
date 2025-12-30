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

#include "display_manager.h"

void DisplayManager::init(DisplayBackend initial) {
  s_lock.init("display_manager_lock");
  spinlock_guard guard(s_lock);
  s_primary = initial;
}

void DisplayManager::set_primary(DisplayBackend be) {
  spinlock_guard guard(s_lock);
  s_primary = be;
}

DisplayBackend DisplayManager::primary() {
  spinlock_guard guard(s_lock);
  return s_primary;
}