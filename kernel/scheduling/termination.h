// unit_termination.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 18.03.26.
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
#ifndef VESPERAOS_UNIT_TERMINATION_H
#define VESPERAOS_UNIT_TERMINATION_H

#include <vespera/signals.h>

namespace kernel::scheduling {
    [[noreturn]] void kill_current_realm(Signal sig, const char* reason);
    i64 kill_realm_by_id(u64 rid, Signal sig);

    /**
     * @brief Terminates the current unit with the given exit code.
     *
     * If this is the last unit in its realm, marks the realm as exited,
     * stores the exit code, and wakes all waiters. Enqueues the unit in
     * the reaper and yields. Never returns.
     */
    [[noreturn]] void exit_current(int exit_code);

    /**
    * @brief Terminates the current realm with the given exit code.
    */
    [[noreturn]] void exit_current_realm(int exit_code);
} // namespace kernel::scheduling

#endif  // VESPERAOS_UNIT_TERMINATION_H
