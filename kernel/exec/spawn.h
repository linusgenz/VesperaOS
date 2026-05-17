// spawn.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.05.26.
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

#ifndef VESPERAOS_VESPERA_EXEC_SPAWN_H
#define VESPERAOS_VESPERA_EXEC_SPAWN_H

#include <klib/result.h>
#include <uapi/vespera/spawn.h>
#include <vespera/types.h>

namespace kernel::exec {

    /**
     * @brief Creates a new realm, loads an ELF binary, and schedules its main unit.
     *
     * Handles credential inheritance, session/TTY setup, handle transfer,
     * and CWD inheritance from the calling realm.
     *
     * @return RealmId of the new realm on success, negative errno on failure.
     */
    [[nodiscard]] Result<RealmId> spawn(
        const char*           path,
        const char**          argv,
        const char**          envp,
        const spawn_config_t* cfg
    );

} // namespace kernel::exec

#endif // VESPERAOS_VESPERA_EXEC_SPAWN_H