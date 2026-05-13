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
#include <filesystem/vfs_node.h>
#include <security/permission.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera_errno.h>

#include "../handle_resolution.h"
#include "uapi/vespera/handles.h"
#include <filesystem/vfs_handle.h>

using namespace kernel::security;

namespace syscalls::internal {
    i64 sys_getuid(u64, u64, u64, u64, u64, u64) {
        const Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;
        return r->cred.uid;
    }

    i64 sys_geteuid(u64, u64, u64, u64, u64, u64) {
        const Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;
        return r->cred.euid;
    }

    i64 sys_setuid(u64 arg0, u64, u64, u64, u64, u64) {
        const u32 new_uid = arg0;

        Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;

        if (is_root(r->cred)) {
            r->cred.uid = new_uid;
            r->cred.euid = new_uid;
            r->cred.suid = new_uid;
            return 0;
        }

        if (!may_set_uid(r->cred, new_uid)) return -EPERM;

        r->cred.euid = new_uid;
        return 0;
    }

    i64 sys_setreuid(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const u32 ruid = arg0;
        const u32 euid = arg1;

        Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;

        const process_credentials old = r->cred;

        if (ruid != static_cast<u32>(-1)) {
            if (!is_root(old) && ruid != old.uid && ruid != old.euid) return -EPERM;
            r->cred.uid = ruid;
        }

        if (euid != static_cast<u32>(-1)) {
            if (!is_root(old) && euid != old.uid && euid != old.euid && euid != old.suid) return -EPERM;
            r->cred.euid = euid;
        }

        // If the real uid was changed, or the euid was set to something other than
        // the old real uid, update the saved set-uid.
        if (ruid != static_cast<u32>(-1) || (euid != static_cast<u32>(-1) && euid != old.uid)) {
            r->cred.suid = r->cred.euid;
        }

        return 0;
    }

    i64 sys_setresuid(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const u32 ruid = arg0;
        const u32 euid = arg1;
        const u32 suid = arg2;

        Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;

        const process_credentials old = r->cred;

        auto valid = [&](u32 v) -> bool {
            if (v == static_cast<u32>(-1)) return true;
            if (is_root(old)) return true;
            return v == old.uid || v == old.euid || v == old.suid;
        };

        if (!valid(ruid) || !valid(euid) || !valid(suid)) return -EPERM;

        if (ruid != static_cast<u32>(-1)) r->cred.uid = ruid;
        if (euid != static_cast<u32>(-1)) r->cred.euid = euid;
        if (suid != static_cast<u32>(-1)) r->cred.suid = suid;

        return 0;
    }

    i64 sys_getresuid(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const auto out_ruid = reinterpret_cast<u32*>(arg0);
        const auto out_euid = reinterpret_cast<u32*>(arg1);
        const auto out_suid = reinterpret_cast<u32*>(arg2);

        if (!out_ruid || !out_euid || !out_suid) return -EINVAL;

        const Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;

        *out_ruid = r->cred.uid;
        *out_euid = r->cred.euid;
        *out_suid = r->cred.suid;
        return 0;
    }

    i64 sys_getgid(u64, u64, u64, u64, u64, u64) {
        const Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;
        return static_cast<i64>(r->cred.gid);
    }

    i64 sys_getegid(u64, u64, u64, u64, u64, u64) {
        const Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;
        return static_cast<i64>(r->cred.egid);
    }

    i64 sys_setgid(u64 arg0, u64, u64, u64, u64, u64) {
        const u32 new_gid = arg0;

        Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;

        if (is_root(r->cred)) {
            r->cred.gid = new_gid;
            r->cred.egid = new_gid;
            r->cred.sgid = new_gid;
            return 0;
        }

        if (!may_set_gid(r->cred, new_gid)) return -EPERM;

        r->cred.egid = new_gid;
        return 0;
    }

    i64 sys_setregid(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const u32 rgid = arg0;
        const u32 egid = arg1;

        Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;

        const process_credentials old = r->cred;

        if (rgid != static_cast<u32>(-1)) {
            if (!is_root(old) && rgid != old.gid && rgid != old.egid) return -EPERM;
            r->cred.gid = rgid;
        }

        if (egid != static_cast<u32>(-1)) {
            if (!is_root(old) && egid != old.gid && egid != old.egid && egid != old.sgid) return -EPERM;
            r->cred.egid = egid;
        }

        if (rgid != static_cast<u32>(-1) || (egid != static_cast<u32>(-1) && egid != old.gid)) {
            r->cred.sgid = r->cred.egid;
        }

        return 0;
    }

    i64 sys_setresgid(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const u32 rgid = arg0;
        const u32 egid = arg1;
        const u32 sgid = arg2;

        Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;

        const process_credentials old = r->cred;

        auto valid = [&](u32 v) -> bool {
            if (v == static_cast<u32>(-1)) return true;
            if (is_root(old)) return true;
            return v == old.gid || v == old.egid || v == old.sgid;
        };

        if (!valid(rgid) || !valid(egid) || !valid(sgid)) return -EPERM;

        if (rgid != static_cast<u32>(-1)) r->cred.gid = rgid;
        if (egid != static_cast<u32>(-1)) r->cred.egid = egid;
        if (sgid != static_cast<u32>(-1)) r->cred.sgid = sgid;

        return 0;
    }

    i64 sys_getresgid(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const auto out_rgid = reinterpret_cast<u32*>(arg0);
        const auto out_egid = reinterpret_cast<u32*>(arg1);
        const auto out_sgid = reinterpret_cast<u32*>(arg2);
        if (!out_rgid || !out_egid || !out_sgid) return -EINVAL;

        const Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;

        *out_rgid = r->cred.gid;
        *out_egid = r->cred.egid;
        *out_sgid = r->cred.sgid;
        return 0;
    }

    i64 sys_chown(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const auto path = reinterpret_cast<const char*>(arg0);
        const u32 uid = arg1;
        const u32 gid = arg2;

        if (!path) return -EINVAL;

        const Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;

        char abs_path[256];
        if (!VFS::resolve_to_absolute(path, abs_path, sizeof(abs_path))) return -EINVAL;

        VfsNode* node = SYSCALL_TRY(VFS::open(abs_path));

        if (const int perm = vfs_check_chown(node, uid, gid, r->cred); perm != 0) {
            VFS::close(node);
            return perm;
        }

        auto res = VFS::chown(node, uid, gid);
        VFS::close(node);
        if (res.is_err()) return res.to_errno();
        return SUCCESS_CODE;
    }

    i64 sys_fchown(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const HandleId hid = arg0;
        const u32 uid = static_cast<u32>(arg1);
        const u32 gid = static_cast<u32>(arg2);

        const auto rh = SYSCALL_TRY(resolve_handle(hid));

        if (rh.type() != HANDLE_TYPE_FILE && rh.type() != HANDLE_TYPE_DIRECTORY) return -EBADH;

        const auto* vh = rh.resource_as<VfsHandle>();
        if (!vh || !vh->node) return -EBADH;

        if (const int perm = vfs_check_chown(vh->node, uid, gid, rh.realm->cred); perm != 0) return perm;

        const auto res = VFS::chown(vh->node, uid, gid);
        return res.is_err() ? res.to_errno() : SUCCESS_CODE;
    }

    i64 sys_chmod(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const auto path = reinterpret_cast<const char*>(arg0);
        const u16 mode = arg1;

        if (!path) return -EINVAL;

        const Realm* r = kernel::scheduling::get_current_realm();
        if (!r) return -ESRCH;

        char abs_path[256];
        if (!VFS::resolve_to_absolute(path, abs_path, sizeof(abs_path))) return -EINVAL;

        VfsNode* node = SYSCALL_TRY(VFS::open(abs_path));

        if (const int perm = vfs_check_chmod(node, mode, r->cred); perm != 0) {
            VFS::close(node);
            return perm;
        }

        auto res = VFS::chmod(node, mode);
        VFS::close(node);
        if (res.is_err()) return res.to_errno();
        return SUCCESS_CODE;
    }

    i64 sys_fchmod(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const HandleId hid = arg0;
        const u16 mode = static_cast<u16>(arg1);

        const auto rh = SYSCALL_TRY(resolve_handle(hid));

        if (rh.type() != HANDLE_TYPE_FILE && rh.type() != HANDLE_TYPE_DIRECTORY) return -EBADH;

        const auto* vh = rh.resource_as<VfsHandle>();
        if (!vh || !vh->node) return -EBADH;

        if (const int perm = vfs_check_chmod(vh->node, mode, rh.realm->cred); perm != 0) return perm;

        const auto res = VFS::chmod(vh->node, mode);
        return res.is_err() ? res.to_errno() : SUCCESS_CODE;
    }
}  // namespace syscalls::internal