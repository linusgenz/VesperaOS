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

static void ref_void(void* p) {
    Channel::ref(static_cast<Channel*>(p));
}

namespace syscalls::internal {

    i64 sys_pipe(u64 arg0, u64, u64, u64, u64, u64) {
        auto* hdls = reinterpret_cast<i64*>(arg0);
        if (!hdls) return -EINVAL;

        Channel* ch = Channel::create(65536);
        if (!ch) return -ENOMEM;

        const Result<HandleId> read_result = kernel::realm::add_handle_to_current(
            HANDLE_TYPE_PIPE,
            ch,
            CAP_READ,
            /*transferable=*/true,
            Channel::destroy,
            ref_void
        );

        if (read_result.is_err()) {
            Channel::destroy(ch);
            return read_result.to_errno();
        }

        Channel::ref(ch);

        const Result<HandleId> write_result = kernel::realm::add_handle_to_current(
            HANDLE_TYPE_PIPE,
            ch,
            CAP_WRITE,
            /*transferable=*/true,
            Channel::destroy,
            ref_void
        );

        if (write_result.is_err()) {
            // TODO WE HAVE TO FREE THIS SOMEHOW HERE
            // kernel::realm::release_handle_from_current(read_result.unwrap());

            return write_result.to_errno();
        }

        hdls[0] = static_cast<i64>(read_result.unwrap());
        hdls[1] = static_cast<i64>(write_result.unwrap());

        return 0;
    }

}  // namespace syscalls::internal