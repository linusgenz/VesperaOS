// credentials.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 26.04.26.
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

#ifndef VESPERAOS_SECURITY_CREDENTIALS_H
#define VESPERAOS_SECURITY_CREDENTIALS_H

#include <klib/result.h>
#include <vespera/types.h>

namespace kernel::security {

    constexpr u32 UID_ROOT = 0;
    constexpr u32 GID_ROOT = 0;

    struct process_credentials {
        u32 uid = UID_ROOT;   ///< real user ID
        u32 gid = GID_ROOT;   ///< real group ID
        u32 euid = UID_ROOT;  ///< effective user ID — used for permission checks
        u32 egid = GID_ROOT;  ///< effective group ID
        u32 suid = UID_ROOT;  ///< saved set-user-ID (restored after temporary priv drop)
        u32 sgid = GID_ROOT;  ///< saved set-group-ID
    };

    inline bool is_root(const process_credentials& cred) {
        return cred.euid == UID_ROOT;
    }

    /// Returns true if the caller may set uid to @p target_uid.
    inline bool may_set_uid(const process_credentials& cred, u32 target_uid) {
        if (is_root(cred)) return true;
        return target_uid == cred.uid || target_uid == cred.euid || target_uid == cred.suid;
    }

    /// Returns true if the caller may set gid to @p target_gid.
    inline bool may_set_gid(const process_credentials& cred, u32 target_gid) {
        if (is_root(cred)) return true;
        return target_gid == cred.gid || target_gid == cred.egid || target_gid == cred.sgid;
    }

    /**
     * @brief Returns a copy of the credentials of the currently running realm.
     */
    [[nodiscard]]
    Result<process_credentials> current_credentials();

    /**
     * @brief Mutates credentials of current realm safely.
     */
    VoidResult set_current_uid(u32 uid);
    VoidResult set_current_euid(u32 euid);

    VoidResult set_current_gid(u32 gid);
    VoidResult set_current_egid(u32 egid);

    VoidResult set_full_credentials(const process_credentials& cred);

}  // namespace kernel::security

#endif  // VESPERAOS_SECURITY_CREDENTIALS_H