// socket_handle.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.09.26.
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

#include <vespera/ipc/socket_handle.h>

SocketHandle* SocketHandle::create() {
    return new SocketHandle();
}

void SocketHandle::ref(void* res) {
    if (!res) return;
    auto* h = static_cast<SocketHandle*>(res);
    __sync_add_and_fetch(&h->refcount, 1);
}

void SocketHandle::destroy(void* res) {
    if (!res) return;
    auto* h = static_cast<SocketHandle*>(res);

    if (__sync_sub_and_fetch(&h->refcount, 1) != 0)
        return;

    switch (h->state) {
        case SocketState::BOUND:
        case SocketState::LISTENING:
            if (h->listener) SocketListener::destroy(h->listener);
            break;
        case SocketState::CONNECTED:
            if (h->endpoint) SocketEndpoint::destroy(h->endpoint);
            break;
        case SocketState::UNBOUND:
        default:
            break;
    }

    delete h;
}
