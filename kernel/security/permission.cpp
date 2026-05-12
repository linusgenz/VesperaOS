// permission.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 27.04.26.
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

#include "permission.h"

#include <vespera_errno.h>

#include <filesystem/vfs/vfs_node.h>
#include "uapi/vespera/stat.h"
#include "filesystem/vfs.h"

namespace kernel::security {

    int vfs_check_permission(const VfsNode* node, const u32 access, const process_credentials& cred) {
        if (!node) return -EINVAL;

        // Root is omnipotent.
        if (is_root(cred)) return 0;

        vespera_stat_t st{};
        if (VFS::stat(node, &st).is_err()) return 0;

        // Determine which 3-bit permission field applies.
        u8 perm_bits = 0;

        if (cred.euid == st.uid) {
            // Owner permissions: bits 8-6 of mode (rwx------).
            perm_bits = static_cast<u8>((st.mode >> 6) & 0x7u);
        } else if (cred.egid == st.gid) {
            // Group permissions: bits 5-3 of mode (---rwx---).
            perm_bits = static_cast<u8>((st.mode >> 3) & 0x7u);
        } else {
            // Other permissions: bits 2-0 of mode (------rwx).
            perm_bits = static_cast<u8>(st.mode & 0x7u);
        }

        if ((access & VFS_ACCESS_READ) && !(perm_bits & 0x4u)) return -EACCES;
        if ((access & VFS_ACCESS_WRITE) && !(perm_bits & 0x2u)) return -EACCES;
        if ((access & VFS_ACCESS_EXEC) && !(perm_bits & 0x1u)) return -EACCES;

        return 0;
    }

    int vfs_check_chown(const VfsNode* node, const u32 new_uid, const u32 new_gid, const process_credentials& cred) {
        if (!node) return -EINVAL;

        // Root may always chown.
        if (is_root(cred)) return 0;

        vespera_stat_t st{};
        if (VFS::stat(node, &st).is_err()) return 0;

        if (new_uid != st.uid) return -EPERM;

        if (cred.euid != st.uid) return -EPERM;
        if (new_gid != cred.egid && new_gid != cred.gid) return -EPERM;

        return 0;
    }

    int vfs_check_chmod(const VfsNode* node, const u16 /*new_mode*/, const process_credentials& cred) {
        if (!node) return -EINVAL;
        if (is_root(cred)) return 0;

        vespera_stat_t st{};
        if (VFS::stat(node, &st).is_err()) return 0;

        if (cred.euid != st.uid) return -EPERM;

        return 0;
    }

}  // namespace kernel::security
