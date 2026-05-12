// sys_stat.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.03.26.
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
#include <kernel/units/unit.h>
#include <vespera_errno.h>

#include <kernel/tty/tty_device.h>
#include <filesystem/vfs/vfs_node.h>

namespace syscalls::internal {

    static u32 node_flags_from_vfs(const VfsNode* n) {
        u32 f = VSTAT_FLAG_READABLE;
        if (n->ops && n->ops->write) f |= VSTAT_FLAG_WRITABLE;
        if (n->type == VfsNodeType::CharDevice || n->type == VfsNodeType::BlockDevice ||
            n->type == VfsNodeType::OtherDevice)
            f |= VSTAT_FLAG_VIRTUAL;
        if (n->permanent) f |= VSTAT_FLAG_PERMANENT;
        return f;
    }

    static u8 vfs_type_to_stat_type(VfsNodeType t) {
        switch (t) {
            case VfsNodeType::File:
                return VSTAT_TYPE_FILE;
            case VfsNodeType::Directory:
                return VSTAT_TYPE_DIR;
            case VfsNodeType::CharDevice:
                return VSTAT_TYPE_CHARDEV;
            case VfsNodeType::BlockDevice:
                return VSTAT_TYPE_BLOCKDEV;
            default:
                return VSTAT_TYPE_UNKNOWN;
        }
    }

    i64 sys_stat(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const auto path = reinterpret_cast<const char*>(arg0);
        auto* out_buf = reinterpret_cast<vespera_stat_t*>(arg1);

        if (!path || !*path || !out_buf) return -EINVAL;

        char norm[256];
        if (!VFS::resolve_to_absolute(path, norm, sizeof(norm))) return -EINVAL;

        VfsNode* node = SYSCALL_TRY(VFS::open(norm));

        vespera_stat_t st{};
        st.node_type = vfs_type_to_stat_type(node->type);
        st.flags = node_flags_from_vfs(node);
        st.size = node->size;

        auto stat_res = VFS::stat(node, &st);
        VFS::close(node);

        if (stat_res.is_err()) return stat_res.to_errno();

        memcpy(out_buf, &st, sizeof(st));
        return SUCCESS_CODE;
    }

}  // namespace syscalls::internal