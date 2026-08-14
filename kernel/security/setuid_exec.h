// setuid_exec.h
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

#ifndef VESPERAOS_SECURITY_SETUID_EXEC_H
#define VESPERAOS_SECURITY_SETUID_EXEC_H

#include <vespera/security/credentials.h>
#include <vespera/types.h>
#include <filesystem/vfs_node.h>
#include "uapi/vespera/stat.h"

namespace kernel::security {

    /// Update @p cred for an exec() of @p exec_node.
    inline void apply_exec_credentials(process_credentials& cred, const VfsNode* exec_node) {
        if (!exec_node || !exec_node->ops || !exec_node->ops->stat) {
            return;
        }

        stat st{};
        if (VFS::stat(exec_node, &st).is_err()) {
            return;
        }

        if (st.st_mode & S_ISUID) {
            cred.euid = st.st_uid;
            cred.suid = st.st_uid;
        } else {
            cred.suid = cred.euid;
        }

        if (st.st_mode & S_ISGID) {
            cred.egid = st.st_gid;
            cred.sgid = st.st_gid;
        } else {
            cred.sgid = cred.egid;
        }
    }

} // namespace kernel::security

#endif  // VESPERAOS_SECURITY_SETUID_EXEC_H
