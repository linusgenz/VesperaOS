// sys_pipe.cpp
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

#include <filesystem/vfs.h>
#include <realm/handle_table.h>
#include <uapi/vespera/handles.h>
#include <vespera/realm/handles.h>
#include <vespera/types.h>

namespace syscalls::internal {

    i64 sys_pipe(u64 arg0, u64, u64, u64, u64, u64) {
        auto* hdls = reinterpret_cast<i64*>(arg0);
        if (!hdls) return -EINVAL;


        const Result<PipePair> pair_result = Channel::create_pipe(65536);
        if (pair_result.is_err()) return pair_result.to_errno();

        const PipePair pair = pair_result.unwrap();

        const Result<HandleId> read_result = kernel::realm::add_handle_to_current(
            HANDLE_TYPE_PIPE,
            pair.read_end,
            CAP_READ,
            /*transferable=*/true,
            ChannelEndpoint::destroy,
            ChannelEndpoint::ref
        );

        if (read_result.is_err()) {
            ChannelEndpoint::destroy(pair.read_end);
            ChannelEndpoint::destroy(pair.write_end);
            return read_result.to_errno();
        }

        const Result<HandleId> write_result = kernel::realm::add_handle_to_current(
            HANDLE_TYPE_PIPE,
            pair.write_end,
            CAP_WRITE,
            /*transferable=*/true,
            ChannelEndpoint::destroy,
            ChannelEndpoint::ref
        );

        if (write_result.is_err()) {
            //  kernel::realm::release_handle_from_current(read_result.unwrap());
            ChannelEndpoint::destroy(pair.write_end);
            return write_result.to_errno();
        }

        hdls[0] = static_cast<i64>(read_result.unwrap());  // read fd
        hdls[1] = static_cast<i64>(write_result.unwrap()); // write fd

        return 0;
    }
} // namespace syscalls::internal
