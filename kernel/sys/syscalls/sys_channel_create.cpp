// sys_channel_create.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 01.10.25.
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

#include <realm/handle_table.h>
#include <uapi/vespera/handles.h>
#include <vespera/ipc/channel.h>
#include <vespera/realm/realm.h>
#include <vespera/scheduling.h>

namespace syscalls::internal {
    i64 sys_channel_create(u64 arg0, u64, u64, u64, u64, u64) {
        usize capacity = arg0;
        if (capacity == 0) capacity = 4096;  // default size

        Realm* realm = kernel::scheduling::get_current_realm();
        if (!realm) return -ESRCH;

        Channel* ch = Channel::create(capacity);
        if (!ch) return -ENOMEM;

        constexpr capability_set caps = CAP_READ | CAP_WRITE;

        const Result<HandleId> result =
            realm->handle_table->add(HANDLE_TYPE_CHANNEL, ch, caps, true, Channel::destroy, nullptr);

        if (result.is_err()) {
            Channel::destroy(ch);
            return result.to_errno();
        }

        return result.unwrap();
    }
}  // namespace syscalls::internal
