// credentials.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 31.05.26.
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


    Result<process_credentials> current_credentials() {
        return Result<process_credentials>::ok(process_credentials{});
    }
}
#endif  // VESPERAOS_SECURITY_CREDENTIALS_H
