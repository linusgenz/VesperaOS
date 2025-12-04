// completion.h
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

#ifndef VESPERAOS_COMPLETION_H
#define VESPERAOS_COMPLETION_H

#include <cstdint>
#include <kernel/sync/spinlock.h>

struct completion_t
{
    volatile bool completed{};
    spinlock_t lock{};

    void init();

    void wait() const;

    [[nodiscard]] bool wait_timeout(uint64_t timeout_ms) const;

    void complete();
};

#endif //VESPERAOS_COMPLETION_H
