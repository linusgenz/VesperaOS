// realm_ops.h
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

#ifndef VESPERAOS_VESPERA_REALM_REALM_OPS_H
#define VESPERAOS_VESPERA_REALM_REALM_OPS_H

#include <klib/result.h>
#include <vespera/types.h>

enum class Signal : u32;

namespace kernel::realm {

    [[nodiscard]] Result<RealmId> get_pgid(RealmId id);
    [[nodiscard]] Result<RealmId> get_sid(RealmId id);
    VoidResult set_pgid(RealmId id, RealmId pgid);

    /**
     * @brief Sends a signal to the first unit in the given realm.
     */
    VoidResult send_signal(RealmId id, Signal sig);

    /**
     * @brief Waits until the given realm exits and returns its exit code.
     *
     * @return Error::Child if the realm does not exist.
     */
    [[nodiscard]]
    Result<int> wait(RealmId id);

}  // namespace kernel::realm

#endif  // VESPERAOS_VESPERA_REALM_REALM_OPS_H