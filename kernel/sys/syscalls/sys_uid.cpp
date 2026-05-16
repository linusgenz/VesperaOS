// sys_uid.cpp
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

#include <filesystem/vfs.h>
#include <filesystem/vfs_handle.h>
#include <filesystem/vfs_node.h>
#include <security/permission.h>
#include <vespera/realm/realm_manager.h>
#include <vespera_errno.h>

#include "../handle_resolution.h"
#include "uapi/vespera/handles.h"

namespace syscalls::internal {
    i64 sys_getuid(u64, u64, u64, u64, u64, u64) {
        auto cred = SYSCALL_TRY(kernel::security::current_credentials());
        return cred.uid;
    }

    i64 sys_geteuid(u64, u64, u64, u64, u64, u64) {
        auto cred = SYSCALL_TRY(kernel::security::current_credentials());
        return cred.euid;
    }

    i64 sys_setuid(u64 arg0, u64, u64, u64, u64, u64) {
        const u32 uid = arg0;

        const auto cred = SYSCALL_TRY(kernel::security::current_credentials());

        if (is_root(cred)) {
            auto c = cred;
            c.uid = uid;
            c.euid = uid;
            c.suid = uid;
            return kernel::security::set_full_credentials(c).to_errno();
        }

        if (!may_set_uid(cred, uid)) return -EPERM;

        return kernel::security::set_current_euid(uid).to_errno();
    }

    i64 sys_setreuid(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const u32 ruid = arg0;
        const u32 euid = arg1;

        auto cred = SYSCALL_TRY(kernel::security::current_credentials());

        const auto old = cred;

        if (ruid != static_cast<u32>(-1)) {
            if (!is_root(old) && ruid != old.uid && ruid != old.euid) return -EPERM;
            cred.uid = ruid;
        }

        if (euid != static_cast<u32>(-1)) {
            if (!is_root(old) && euid != old.uid && euid != old.euid && euid != old.suid) return -EPERM;
            cred.euid = euid;
        }

        if (ruid != static_cast<u32>(-1) || (euid != static_cast<u32>(-1) && euid != old.uid)) {
            cred.suid = cred.euid;
        }

        return kernel::security::set_full_credentials(cred).to_errno();
    }

    i64 sys_getresuid(u64 a, u64 b, u64 c, u64, u64, u64) {
        auto* ru = reinterpret_cast<u32*>(a);
        auto* eu = reinterpret_cast<u32*>(b);
        auto* su = reinterpret_cast<u32*>(c);

        if (!ru || !eu || !su) return -EINVAL;

        auto cred = SYSCALL_TRY(kernel::security::current_credentials());

        *ru = cred.uid;
        *eu = cred.euid;
        *su = cred.suid;

        return 0;
    }

    i64 sys_setresuid(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const u32 ruid = arg0;
        const u32 euid = arg1;
        const u32 suid = arg2;

        auto cred = SYSCALL_TRY(kernel::security::current_credentials());
        const auto old = cred;

        auto valid = [&](u32 v) -> bool {
            if (v == static_cast<u32>(-1)) return true;

            if (is_root(old)) return true;

            return v == old.uid || v == old.euid || v == old.suid;
        };

        if (!valid(ruid) || !valid(euid) || !valid(suid)) return -EPERM;

        if (ruid != static_cast<u32>(-1)) cred.uid = ruid;
        if (euid != static_cast<u32>(-1)) cred.euid = euid;
        if (suid != static_cast<u32>(-1)) cred.suid = suid;

        return kernel::security::set_full_credentials(cred).to_errno();
    }

    i64 sys_getgid(u64, u64, u64, u64, u64, u64) {
        const auto cred = SYSCALL_TRY(kernel::security::current_credentials());
        return static_cast<i64>(cred.gid);
    }

    i64 sys_getegid(u64, u64, u64, u64, u64, u64) {
        const auto cred = SYSCALL_TRY(kernel::security::current_credentials());
        return static_cast<i64>(cred.egid);
    }

    i64 sys_setgid(u64 arg0, u64, u64, u64, u64, u64) {
        const u32 new_gid = arg0;

        const auto cred = SYSCALL_TRY(kernel::security::current_credentials());

        if (is_root(cred)) {
            auto c = cred;
            c.gid = new_gid;
            c.egid = new_gid;
            c.sgid = new_gid;
            return kernel::security::set_full_credentials(c).to_errno();
        }

        if (!may_set_gid(cred, new_gid)) return -EPERM;

        return kernel::security::set_current_egid(new_gid).to_errno();
    }

    i64 sys_setregid(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const u32 rgid = arg0;
        const u32 egid = arg1;

        auto cred = SYSCALL_TRY(kernel::security::current_credentials());
        const auto old = cred;

        if (rgid != static_cast<u32>(-1)) {
            if (!is_root(old) && rgid != old.gid && rgid != old.egid) return -EPERM;
            cred.gid = rgid;
        }

        if (egid != static_cast<u32>(-1)) {
            if (!is_root(old) && egid != old.gid && egid != old.egid && egid != old.sgid) return -EPERM;

            cred.egid = egid;
        }

        if (rgid != static_cast<u32>(-1) || (egid != static_cast<u32>(-1) && egid != old.gid)) {
            cred.sgid = cred.egid;
        }

        return kernel::security::set_full_credentials(cred).to_errno();
    }

    i64 sys_setresgid(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const u32 rgid = arg0;
        const u32 egid = arg1;
        const u32 sgid = arg2;

        auto cred = SYSCALL_TRY(kernel::security::current_credentials());
        const auto old = cred;

        auto valid = [&](u32 v) {
            if (v == static_cast<u32>(-1)) return true;
            if (is_root(old)) return true;

            return v == old.gid || v == old.egid || v == old.sgid;
        };

        if (!valid(rgid) || !valid(egid) || !valid(sgid)) return -EPERM;

        if (rgid != static_cast<u32>(-1)) cred.gid = rgid;
        if (egid != static_cast<u32>(-1)) cred.egid = egid;
        if (sgid != static_cast<u32>(-1)) cred.sgid = sgid;

        return kernel::security::set_full_credentials(cred).to_errno();
    }

    i64 sys_getresgid(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        auto* out_rgid = reinterpret_cast<u32*>(arg0);
        auto* out_egid = reinterpret_cast<u32*>(arg1);
        auto* out_sgid = reinterpret_cast<u32*>(arg2);

        if (!out_rgid || !out_egid || !out_sgid) return -EINVAL;

        const auto cred = SYSCALL_TRY(kernel::security::current_credentials());

        *out_rgid = cred.gid;
        *out_egid = cred.egid;
        *out_sgid = cred.sgid;

        return 0;
    }

    i64 sys_chown(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const auto path = reinterpret_cast<const char*>(arg0);
        const u32 uid = arg1;
        const u32 gid = arg2;

        if (!path) return -EINVAL;

        const auto cred = SYSCALL_TRY(kernel::security::current_credentials());

        char abs_path[256];
        if (!VFS::resolve_to_absolute(path, abs_path, sizeof(abs_path))) return -EINVAL;

        VfsNode* node = SYSCALL_TRY(VFS::open(abs_path));

        if (const int perm = vfs_check_chown(node, uid, gid, cred); perm != 0) {
            VFS::close(node);
            return perm;
        }

        auto res = VFS::chown(node, uid, gid);
        VFS::close(node);

        return res.is_err() ? res.to_errno() : SUCCESS_CODE;
    }

    i64 sys_fchown(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const HandleId hid = arg0;
        const u32 uid = arg1;
        const u32 gid = arg2;

        const auto rh = SYSCALL_TRY(resolve_handle(hid));

        if (rh.type() != HANDLE_TYPE_FILE && rh.type() != HANDLE_TYPE_DIRECTORY) return -EBADH;

        const auto* vh = rh.resource_as<VfsHandle>();
        if (!vh || !vh->node) return -EBADH;

        const auto cred = SYSCALL_TRY(kernel::security::current_credentials());

        if (const int perm = vfs_check_chown(vh->node, uid, gid, cred); perm != 0) return perm;

        const auto res = VFS::chown(vh->node, uid, gid);
        return res.is_err() ? res.to_errno() : SUCCESS_CODE;
    }

    i64 sys_chmod(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const auto path = reinterpret_cast<const char*>(arg0);
        const u16 mode = arg1;

        if (!path) return -EINVAL;

        const auto cred = SYSCALL_TRY(kernel::security::current_credentials());

        char abs_path[256];
        if (!VFS::resolve_to_absolute(path, abs_path, sizeof(abs_path))) return -EINVAL;

        VfsNode* node = SYSCALL_TRY(VFS::open(abs_path));

        if (const int perm = vfs_check_chmod(node, mode, cred); perm != 0) {
            VFS::close(node);
            return perm;
        }

        auto res = VFS::chmod(node, mode);
        VFS::close(node);

        return res.is_err() ? res.to_errno() : SUCCESS_CODE;
    }

    i64 sys_fchmod(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const HandleId hid = arg0;
        const u16 mode = static_cast<u16>(arg1);

        const auto rh = SYSCALL_TRY(resolve_handle(hid));

        if (rh.type() != HANDLE_TYPE_FILE && rh.type() != HANDLE_TYPE_DIRECTORY) return -EBADH;

        const auto* vh = rh.resource_as<VfsHandle>();
        if (!vh || !vh->node) return -EBADH;

        const auto cred = SYSCALL_TRY(kernel::security::current_credentials());

        if (const int perm = vfs_check_chmod(vh->node, mode, cred); perm != 0) return perm;

        const auto res = VFS::chmod(vh->node, mode);
        return res.is_err() ? res.to_errno() : SUCCESS_CODE;
    }
}  // namespace syscalls::internal