// credentials.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 14.05.26.
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

#include <vespera/security/credentials.h>
#include <vespera/scheduling.h>
#include <realm/realm.h>

namespace kernel::security {

    static process_credentials& current_cred() {
        Realm* r = kernel::scheduling::get_current_realm();
        // Kernel guarantee: only called in syscall context
        return r->cred;
    }

    Result<process_credentials> current_credentials() {
        Realm* r = kernel::scheduling::get_current_realm();
        if (!r)
            return Error::Srch;

        return Result<process_credentials>::ok(r->cred);
    }

    VoidResult set_current_uid(u32 uid) {
        Realm* r = kernel::scheduling::get_current_realm();
        if (!r)
            return Error::Srch;

        if (is_root(r->cred)) {
            r->cred.uid = uid;
            r->cred.euid = uid;
            r->cred.suid = uid;
            return VoidResult::ok();
        }

        if (!may_set_uid(r->cred, uid))
            return Error::Perm;

        r->cred.euid = uid;
        return VoidResult::ok();
    }

    VoidResult set_current_gid(u32 gid) {
        Realm* r = kernel::scheduling::get_current_realm();
        if (!r)
            return Error::Srch;

        if (is_root(r->cred)) {
            r->cred.gid = gid;
            r->cred.egid = gid;
            r->cred.sgid = gid;
            return VoidResult::ok();
        }

        if (!may_set_gid(r->cred, gid))
            return Error::Perm;

        r->cred.egid = gid;
        return VoidResult::ok();
    }

    VoidResult set_current_euid(const u32 euid) {
        Realm* realm = kernel::scheduling::get_current_realm();
        if (!realm)
            return Error::Srch;

        const process_credentials old = realm->cred;

        if (!may_set_uid(old, euid))
            return Error::Perm;

        realm->cred.euid = euid;
        return VoidResult::ok();
    }

    VoidResult set_current_egid(const u32 egid) {
        Realm* realm = kernel::scheduling::get_current_realm();
        if (!realm)
            return Error::Srch;

        const process_credentials old = realm->cred;

        if (!may_set_gid(old, egid))
            return Error::Perm;

        realm->cred.egid = egid;
        return VoidResult::ok();
    }

    VoidResult set_full_credentials(const process_credentials& c) {
        Realm* r = kernel::scheduling::get_current_realm();
        if (!r)
            return Error::Srch;

        const auto old = r->cred;

        auto valid = [&](u32 v, bool is_uid) {
            if (v == (u32)-1) return true;
            if (is_root(old)) return true;
            return is_uid
                ? (v == old.uid || v == old.euid || v == old.suid)
                : (v == old.gid || v == old.egid || v == old.sgid);
        };

        if (!valid(c.uid, true) || !valid(c.euid, true) || !valid(c.suid, true))
            return Error::Perm;

        if (!valid(c.gid, false) || !valid(c.egid, false) || !valid(c.sgid, false))
            return Error::Perm;

        r->cred = c;
        return VoidResult::ok();
    }

} // namespace kernel::security