// spawn.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 20.03.26.
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
#ifndef VESPERAOS_SPAWN_H
#define VESPERAOS_SPAWN_H
#include <vespera/types.h>

/**
 * Optional configuration for sys_spawn.
 * Pass nullptr to get default behavior (inherit TTY stdin/stdout/stderr).
 *
 * Handle IDs refer to handles in the CALLING realm.
 * Use 0 to mean "use default" for each field.
 */
typedef struct spawn_config {
    i64 stdin_handle;   ///< Replace HANDLE_STDIN  in child (0 = inherit TTY)
    i64 stdout_handle;  ///< Replace HANDLE_STDOUT in child (0 = inherit TTY)
    i64 stderr_handle;  ///< Replace HANDLE_STDERR in child (0 = inherit TTY)
    u8  bg_realm;       ///< If set true, detach from controlling tty
    char* realm_name;
} spawn_config_t;

#endif  // VESPERAOS_SPAWN_H
