// jobctl.h
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

#ifndef VESPERAOS_VESPERA_JOBCTL_JOBCTL_H
#define VESPERAOS_VESPERA_JOBCTL_JOBCTL_H

#include <klib/result.h>
#include <vespera/types.h>

class TtyDevice;

namespace kernel::jobctl {

    /**
     * @brief Assigns a controlling TTY to a realm (TIOCSCTTY).
     *
     * Requires the realm to be its own session leader and have no TTY yet.
     *
     * @return Error::Perm if preconditions are not met.
     */
    VoidResult assign_controlling_tty(RealmId realm_id, TtyDevice* tty);

    /**
     * @brief Returns true if @p tty is the controlling TTY of the given realm.
     */
    [[nodiscard]] bool is_controlling_tty(RealmId realm_id, const TtyDevice* tty);

    /**
     * @brief Sets the foreground process group on a TTY (tcsetpgrp).
     *
     * Requires @p tty to be the controlling TTY of @p realm_id.
     *
     * @return Error::Perm if the TTY is not the controlling TTY.
     */
    VoidResult set_foreground_pgid(RealmId realm_id, TtyDevice* tty, RealmId new_pgid);

    /**
     * @brief Returns the current foreground process group of a TTY (tcgetpgrp).
     *
     * @return Error::NotTty if @p tty has no line discipline.
     */
    [[nodiscard]] Result<RealmId> get_foreground_pgid(const TtyDevice* tty);

    /**
     * @brief Creates a new session for the given realm (setsid).
     *
     * The realm becomes:
     *  - session leader
     *  - process group leader
     *
     * Any controlling TTY is detached.
     *
     * @return Error::Perm if the realm is already a session leader.
     */
    [[nodiscard]] Result<RealmId> create_session(RealmId realm_id);

    /**
     * @brief Sets process group ID of a realm.
     *
     * POSIX rules:
     * - session leaders cannot change pgid
     * - target must be in same session as caller
     * - pgid must refer to an existing process group in same session
     */
    [[nodiscard]] VoidResult set_pgid(RealmId caller, RealmId target, RealmId desired_pgid);

}  // namespace kernel::jobctl

#endif  // VESPERAOS_VESPERA_JOBCTL_JOBCTL_H