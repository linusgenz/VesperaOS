// handles.h
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

#ifndef VESPERAOS_VESPERA_REALM_HANDLES_H
#define VESPERAOS_VESPERA_REALM_HANDLES_H

#include <klib/result.h>
#include <uapi/vespera/capabilities.h>
#include <uapi/vespera/handles.h>
#include <vespera/types.h>

class Realm;

namespace kernel::realm {

    [[nodiscard]] Result<HandleId> add_handle(
        Realm*         realm,
        u64            type,
        void*          resource,
        capability_set caps,
        bool           transferable,
        void         (*destroy)(void*),
        void         (*acquire)(void*) = nullptr);

    [[nodiscard]] Result<HandleId> add_handle_to_current(
        u64            type,
        void*          resource,
        capability_set caps,
        bool           transferable,
        void         (*destroy)(void*),
        void         (*acquire)(void*) = nullptr);

} // namespace kernel::realm

#endif // VESPERAOS_VESPERA_REALM_HANDLES_H