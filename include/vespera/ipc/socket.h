// socket.h
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

#ifndef VESPERAOS_IPC_SOCKET_H
#define VESPERAOS_IPC_SOCKET_H
#include "channel.h"

struct SocketEndpoint;

struct SocketPair {
    SocketEndpoint* a;
    SocketEndpoint* b;
};

struct SocketEndpoint {
    ChannelEndpoint* tx{nullptr};   // write side: local -> peer
    ChannelEndpoint* rx{nullptr};   // read side: peer -> local
    int refcount{1};

    static Result<SocketPair> create_connected_pair(usize capacity);
    static void destroy(void* res);
    static void ref(void* res);

    isize send(const void* data, usize len, bool blocking) const;
    isize recv(void* out, usize len, bool blocking) const;

    [[nodiscard]] int poll() const;

private:
    SocketEndpoint(ChannelEndpoint* tx, ChannelEndpoint* rx)
        : tx(tx), rx(rx) {
    }

};

#endif //VESPERAOS_IPC_SOCKET_H
