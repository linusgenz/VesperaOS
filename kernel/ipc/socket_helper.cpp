// socket_helper.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 23.09.26.
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

#include <klib/result.h>
#include <klib/string.h>
#include <uapi/vespera/socket.h>
#include <uapi/vespera/un.h>

VoidResult extract_unix_path(const sockaddr* addr, const socklen_t addrlen, char* out, const usize out_size) {
    if (!addr) return Error::Inval;
    if (addrlen < sizeof(sa_family_t)) return Error::Inval;
    if (addrlen > sizeof(sockaddr_un)) return Error::Inval;

    if (addr->sa_family != AF_UNIX) return Error::AfNoSupport;

    const auto* un = reinterpret_cast<const sockaddr_un*>(addr);

    usize avail = addrlen - offsetof(sockaddr_un, sun_path);
    if (avail > sizeof(un->sun_path)) avail = sizeof(un->sun_path);
    if (avail == 0) return Error::Inval;

    usize len = 0;
    while (len < avail && un->sun_path[len] != '\0') len++;
    if (len == 0) return Error::Inval;
    if (len >= out_size) return Error::NameTooLong;

    memcpy(out, un->sun_path, len);
    out[len] = '\0';

    return VoidResult::ok();
}