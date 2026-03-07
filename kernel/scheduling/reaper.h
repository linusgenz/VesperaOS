/**
 * @file reaper.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 07.12.25.
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
#ifndef VESPERAOS_REAPER_H
#define VESPERAOS_REAPER_H

#include "../units/unit.h"
#include <klib/intrusive_queue.h>

[[noreturn]] void reaper_unit(void* arg);

struct Reaper {
   public:
    Reaper() = default;

    void enqueue(Unit* unit);

    void reap();

    [[nodiscard]] bool empty() const;

   private:
    IntrusiveQueue<Unit> pending_;
};

#endif  // VESPERAOS_REAPER_H