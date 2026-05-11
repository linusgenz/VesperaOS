// sys_ioctl.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 21.09.25.
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

#include <uapi/vespera/dev/ioctl_tty.h>
#include <uapi/vespera/handles.h>
#include <vespera/devices/char_device.h>
#include <vespera/scheduling.h>

#include "../../../filesystem/vfs/vfs_handle.h"
#include "../handle_resolution.h"
#include "../../tty/tty_device.h"

namespace syscalls::internal {
    i64 sys_ioctl(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const HandleId hid = arg0;
        const u64 req = arg1;
        const auto arg = reinterpret_cast<void*>(arg2);

        const auto rh = SYSCALL_TRY(resolve_handle(hid, /*type_mask=*/0, CAP_DEVICE_ACCESS));

        // TIOCSCTTY is special: must be session leader, no controlling TTY yet.
        if (rh.type() == HANDLE_TYPE_TTY && req == TIOCSCTTY) {
            Realm* realm = rh.realm;
            if (realm->sid != realm->id) return -EPERM;
            if (realm->controlling_tty != nullptr) return -EPERM;
            auto* tty_dev = rh.resource_as<TtyDevice>();
            if (!tty_dev) return -ENOTTY;
            realm->controlling_tty = tty_dev;
            return 0;
        }

        switch (rh.type()) {
            case HANDLE_TYPE_DEVICE: {
                const auto* vh = rh.resource_as<VfsHandle>();
                if (!vh || !vh->node || !vh->node->ops || !vh->node->ops->ioctl) return -ENOTTY;
                return vh->node->ops->ioctl(vh->node, req, arg);
            }
            case HANDLE_TYPE_TTY: {
                auto* tty_dev = rh.resource_as<TtyDevice>();
                if (!tty_dev) return -ENODEV;
                CharFile cf{};
                cf.driver_private = tty_dev;
                return tty_dev->ioctl(&cf, static_cast<u32>(req), arg);
            }
            case HANDLE_TYPE_FILE:
                return -ENOTTY;
            default:
                return -EBADH;
        }
    }
}  // namespace syscalls::internal
