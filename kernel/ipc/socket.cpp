// socket.cpp
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


#include <vespera/ipc/socket.h>
#include <vespera_errno.h>

#include "klib/result.h"

namespace {
    void unwind_channel(ChannelEndpoint* a_side, ChannelEndpoint* b_side) {
        if (a_side) ChannelEndpoint::destroy(a_side);
        if (b_side) ChannelEndpoint::destroy(b_side);
    }
}

Result<SocketPair> SocketEndpoint::create_connected_pair(const usize capacity) {
    Channel* ch_a_to_b = Channel::create(capacity);
    if (!ch_a_to_b) return Result<SocketPair>::err(Error::NoMem);

    ChannelEndpoint* a_tx = ChannelEndpoint::create_with_channel(ch_a_to_b, /*r=*/false, /*w=*/true);
    if (!a_tx) {
        Channel::destroy(ch_a_to_b);
        return Result<SocketPair>::err(Error::NoMem);
    }

    ChannelEndpoint* b_rx = ChannelEndpoint::create_with_channel(ch_a_to_b, /*r=*/true, /*w=*/false);
    if (!b_rx) {
        unwind_channel(a_tx, nullptr);
        return Result<SocketPair>::err(Error::NoMem);
    }

    // Direction 2: B -> A (B writes, A reads)
    Channel* ch_b_to_a = Channel::create(capacity);
    if (!ch_b_to_a) {
        unwind_channel(a_tx, b_rx);
        return Result<SocketPair>::err(Error::NoMem);
    }

    ChannelEndpoint* b_tx = ChannelEndpoint::create_with_channel(ch_b_to_a, /*r=*/false, /*w=*/true);
    if (!b_tx) {
        unwind_channel(a_tx, b_rx);
        Channel::destroy(ch_b_to_a);
        return Result<SocketPair>::err(Error::NoMem);
    }

    ChannelEndpoint* a_rx = ChannelEndpoint::create_with_channel(ch_b_to_a, /*r=*/true, /*w=*/false);
    if (!a_rx) {
        unwind_channel(a_tx, b_rx);
        ChannelEndpoint::destroy(b_tx);
        return Result<SocketPair>::err(Error::NoMem);
    }

    auto* a = new SocketEndpoint(a_tx, a_rx);
    if (!a) {
        unwind_channel(a_tx, b_rx);
        unwind_channel(b_tx, a_rx);
        return Result<SocketPair>::err(Error::NoMem);
    }

    auto* b = new SocketEndpoint(b_tx, b_rx);
    if (!b) {
        delete a; // frees the struct only; endpoints already unwound below
        unwind_channel(a_tx, b_rx);
        unwind_channel(b_tx, a_rx);
        return Result<SocketPair>::err(Error::NoMem);
    }

    return Result<SocketPair>::ok({a, b});
}

void SocketEndpoint::ref(void* res) {
    if (!res) return;
    auto* ep = static_cast<SocketEndpoint*>(res);
    __sync_add_and_fetch(&ep->refcount, 1);
}

void SocketEndpoint::destroy(void* res) {
    if (!res) return;
    auto* ep = static_cast<SocketEndpoint*>(res);

    if (__sync_sub_and_fetch(&ep->refcount, 1) != 0)
        return;

    ChannelEndpoint::destroy(ep->tx);
    ChannelEndpoint::destroy(ep->rx);

    delete ep;
}

isize SocketEndpoint::send(const void* data, const usize len, const bool blocking) const {
    if (!tx) return -EBADH;
    return tx->channel->send(data, len, blocking);
}

isize SocketEndpoint::recv(void* out, const usize len, const bool blocking) const {
    if (!rx) return -EBADH;
    return rx->channel->recv(out, len, blocking);
}

int SocketEndpoint::poll() const {
    int mask = 0;
    if (rx) mask |= rx->channel->poll(/*is_reader=*/true, /*is_writer=*/false);
    if (tx) mask |= tx->channel->poll(/*is_reader=*/false, /*is_writer=*/true);
    return mask;
}
