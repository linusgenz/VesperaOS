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

#include <klib/result.h>
#include <uapi/vespera/capabilities.h>
#include <uapi/vespera/handles.h>
#include <vespera/ipc/channel.h>
#include <vespera/realm/handles.h>
#include <vespera/scheduling.h>
#include <vespera_errno.h>

namespace syscalls::internal {
    i64 sys_channel_create(u64 arg0, u64, u64, u64, u64, u64) {
        usize capacity = arg0;
        if (capacity == 0) capacity = 4096; // default size

        Realm* realm = kernel::scheduling::get_current_realm();
        if (!realm) return -ESRCH;

        ChannelEndpoint* ep = ChannelEndpoint::create(capacity, /*r=*/true, /*w=*/true);
        if (!ep) return -ENOMEM;

        constexpr capability_set caps = CAP_READ | CAP_WRITE;

        const Result<HandleId> result = kernel::realm::add_handle_to_current(
            HANDLE_TYPE_CHANNEL,
            ep,
            caps,
            /*transferable=*/true,
            ChannelEndpoint::destroy,
            ChannelEndpoint::ref
        );

        if (result.is_err()) {
            ChannelEndpoint::destroy(ep);
            return result.to_errno();
        }

        return static_cast<i64>(result.unwrap());
    }
} // namespace syscalls::internal
