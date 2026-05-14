// realm_types.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 14.05.26.
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

#ifndef VESPERAOS_VESPERA_REALM_REALM_TYPES_H
#define VESPERAOS_VESPERA_REALM_REALM_TYPES_H

#include <vespera/types.h>

namespace kernel::realm {

    /** @brief System realm */
    constexpr u8 REALM_SYSTEM = 1;

    /** @brief Driver realm */
    constexpr u8 REALM_DRIVER = 2;

    // Trampoline page - mapped read-only into every user realm.
    constexpr uptr TRAMPOLINE_VADDR = 0x00007FFFFE000000ULL;
    constexpr uptr TRAMP_SIGNAL_OFF = 0x000;
    constexpr uptr TRAMP_UNIT_OFF = 0x100;
    constexpr uptr SIGNAL_TRAMPOLINE_VADDR = (TRAMPOLINE_VADDR + TRAMP_SIGNAL_OFF);
    constexpr uptr USER_UNIT_TRAMPOLINE_VADDR = (TRAMPOLINE_VADDR + TRAMP_UNIT_OFF);

}  // namespace kernel::realm

#endif  // VESPERAOS_VESPERA_REALM_REALM_TYPES_H