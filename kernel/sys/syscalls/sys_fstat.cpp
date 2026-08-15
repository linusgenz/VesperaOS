// sys_fstat.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.08.26.
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

#include <uapi/vespera/stat.h>
#include <filesystem/vfs.h>
#include <vespera/scheduling.h>
#include <vespera_errno.h>
#include <filesystem/vfs_node.h>
#include "filesystem/vfs_handle.h"
#include "sys/handle_resolution.h"

namespace syscalls::internal {
    i64 sys_fstat(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const auto hid = static_cast<HandleId>(arg0);
        auto* out_buf = reinterpret_cast<stat*>(arg1);

        if (!out_buf) return -EINVAL;

        const auto rh = SYSCALL_TRY(resolve_handle(hid, /*type_mask=*/0, CAP_READ));

        VfsNode* node = nullptr;

        switch (rh.type()) {
            case HANDLE_TYPE_DEVICE:
            case HANDLE_TYPE_FILE: {
                const auto* vh = rh.resource_as<VfsHandle>();
                if (!vh || !vh->node) return -EBADH;
                node = vh->node;
                break;
            }
            case HANDLE_TYPE_PIPE: {
                // TODO not implemented yet
                return -ENOSYS;
            }
            default:
                return -EBADH;
        }

        stat st{};
        SYSCALL_TRY_VOID(VFS::stat(node, &st));

        memcpy(out_buf, &st, sizeof(st));
        return SUCCESS_CODE;
    }
}
