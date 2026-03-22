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

#include <uapi/vespera/handels.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>

#include <vespera/filesystem/vfs.h>
#include "../../../include/vespera/types.h"

static void ref_void(void* p) { Channel::ref(static_cast<Channel*>(p)); }

namespace syscalls::internal {
    i64 sys_pipe(u64 arg0, u64, u64, u64, u64, u64) {
        auto* hdls = reinterpret_cast<i64*>(arg0);  // fds[0] = read, fds[1] = write
        if (!hdls) return -EINVAL;

        const Unit* u = kernel::scheduling::get_current_unit();
        Realm* realm = RealmManager::get(u->rid);
        if (!realm) return -EINVAL;

        Channel* ch = Channel::create(65536);
        if (!ch) return -ENOMEM;

        HandleId read_hid = 0, write_hid = 0;

        if (realm->add_handle(HANDLE_TYPE_PIPE, ch, CAP_READ, true, Channel::destroy, ref_void, &read_hid) != SUCCESS_CODE) {
            Channel::destroy(ch);
            return -ENOMEM;
        }

        Channel::ref(ch);
        if (realm->add_handle(HANDLE_TYPE_PIPE, ch, CAP_WRITE, true, Channel::destroy, ref_void, &write_hid) != SUCCESS_CODE) {
            realm->release_handle(read_hid);
            return -ENOMEM;
        }

        hdls[0] = read_hid;
        hdls[1] = write_hid;
        return 0;
    }
}  // namespace syscalls::internal
