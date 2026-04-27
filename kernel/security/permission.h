// permission.h
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

#ifndef VESPERAOS_SECURITY_PERMISSION_H
#define VESPERAOS_SECURITY_PERMISSION_H

#include <vespera/security/credentials.h>
#include <vespera/types.h>

struct VfsNode;

namespace kernel::security {

    constexpr u32 VFS_ACCESS_READ = 0x4u;
    constexpr u32 VFS_ACCESS_WRITE = 0x2u;
    constexpr u32 VFS_ACCESS_EXEC = 0x1u;

    /// Check whether @p cred is allowed to access @p node with the requested @p access mask
    [[nodiscard]] int vfs_check_permission(const VfsNode* node, u32 access, const process_credentials& cred);

    [[nodiscard]] inline int vfs_check_read(const VfsNode* node, const process_credentials& cred) {
        return vfs_check_permission(node, VFS_ACCESS_READ, cred);
    }

    [[nodiscard]] inline int vfs_check_write(const VfsNode* node, const process_credentials& cred) {
        return vfs_check_permission(node, VFS_ACCESS_WRITE, cred);
    }

    [[nodiscard]] inline int vfs_check_exec(const VfsNode* node, const process_credentials& cred) {
        return vfs_check_permission(node, VFS_ACCESS_EXEC, cred);
    }

    /// Check whether @p cred may call chown(node, new_uid, new_gid).
    [[nodiscard]] int vfs_check_chown(const VfsNode* node, u32 new_uid, u32 new_gid, const process_credentials& cred);

    /// Check whether @p cred may call chmod(node, new_mode).
    [[nodiscard]] int vfs_check_chmod(const VfsNode* node, u16 new_mode, const process_credentials& cred);

}  // namespace kernel::security

#endif  // VESPERAOS_SECURITY_PERMISSION_H
