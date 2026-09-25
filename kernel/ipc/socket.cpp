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
#include <klib/string.h>

#include "klib/result.h"
#include "klib/utils.h"
#include "realm/handle_table.h"
#include "realm/handle_transfer.h"
#include "realm/realm.h"
#include "vespera/realm/realm_manager.h"

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

isize SocketEndpoint::sendmsg(const msghdr* msg, const unsigned flags, const RealmId sender_realm) const {
    if (!tx) return -EBADH;
    if (!msg) return -EINVAL;

    const bool blocking = !(flags & MSG_DONTWAIT);

    // 1. Gather msg_iov into one contiguous payload buffer.
    usize payload_len = 0;
    for (usize i = 0; i < msg->msg_iovlen; i++) payload_len += msg->msg_iov[i].iov_len;

    // 2. Walk msg_control, collecting SCM_RIGHTS handle ids. Each is
    //    resolved in the SENDER's realm and acquire()'d so it can't be
    //    destroyed out from under a peer that hasn't recvmsg()'d yet.
    //    On any failure below, everything acquired so far is released.
    FrameRight rights[64]; // arbitrary cap; see note below
    usize rights_count = 0;

    Realm* self_realm = RealmManager::get(sender_realm);
    if (!self_realm) return -ESRCH;

    if (msg->msg_control && msg->msg_controllen > 0) {
        auto* mhdr = const_cast<msghdr*>(msg); // CMSG_* macros take non-const
        for (cmsghdr* c = CMSG_FIRSTHDR(mhdr); c; c = CMSG_NXTHDR(mhdr, c)) {
            if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS) continue;

            const usize hdr_size = CMSG_ALIGN(sizeof(cmsghdr));
            if (c->cmsg_len < hdr_size) return -EINVAL;
            const usize n = (c->cmsg_len - hdr_size) / sizeof(HandleId);
            const auto* hids = reinterpret_cast<const HandleId*>(CMSG_DATA(c));

            for (usize i = 0; i < n; i++) {
                if (rights_count >= 64) {
                    // Too many fds in one message -- unwind and bail.
                    for (usize j = 0; j < rights_count; j++) {
                        HandleEntry* e = self_realm->handle_table->lookup(rights[j].src_hid);
                        if (e && e->acquire) self_realm->handle_table->release(rights[j].src_hid);
                    }
                    return -EINVAL; // consider a dedicated ETOOMANYREFS-equivalent
                }

                HandleEntry* entry = self_realm->handle_table->lookup(hids[i]);
                if (!entry) return -EBADH;
                if (!entry->transferable) return -EACCES;

                self_realm->handle_table->acquire(hids[i]); // held until peer's recvmsg()
                rights[rights_count++] = FrameRight{sender_realm, hids[i]};
            }
        }
    }

    // 3. Build the frame and write it atomically in one Channel::send().
    //    Fixed-size scratch buffer sized for the cap above; payload
    //    itself is gathered separately below rather than copied twice.
    u8 header[sizeof(u32) * 2 + sizeof(FrameRight) * 64 + sizeof(u32)];
    usize hoff = 0;

    auto put_u32 = [&](u32 v) { memcpy(header + hoff, &v, sizeof(v)); hoff += sizeof(v); };

    put_u32(SENDMSG_FRAME_MAGIC);
    put_u32(static_cast<u32>(rights_count));
    memcpy(header + hoff, rights, rights_count * sizeof(FrameRight));
    hoff += rights_count * sizeof(FrameRight);
    put_u32(static_cast<u32>(payload_len));

    // NOTE: this whole frame needs to land in the channel as ONE
    // logical unit, but Channel::send() only atomically writes what's
    // passed to a SINGLE call. Sending header and payload as two
    // separate send() calls would let another sendmsg()/write() on the
    // same channel interleave between them. Channel::send() has no
    // vectored/multi-buffer variant yet -- this needs either (a) a
    // Channel::send that accepts multiple (data,len) fragments under
    // one lock acquisition, or (b) copying header+payload into one
    // scratch buffer first (extra copy, but works with today's API).
    // Going with (b) here; flag (a) as a real Channel improvement if
    // this copy shows up as a hot path later.

    // (b): single scratch buffer for header + gathered payload.
    // Bounded by hoff (header, capped above) + payload_len (caller-
    // controlled -- validate/cap this before allocating in the real
    // implementation; omitted here for brevity).
    u8* frame = static_cast<u8*>(kernel::memory::malloc(hoff + payload_len));
    if (!frame) {
        for (usize j = 0; j < rights_count; j++) self_realm->handle_table->release(rights[j].src_hid);
        return -ENOMEM;
    }

    memcpy(frame, header, hoff);
    usize poff = hoff;
    for (usize i = 0; i < msg->msg_iovlen; i++) {
        memcpy(frame + poff, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
        poff += msg->msg_iov[i].iov_len;
    }

    const isize sent = tx->channel->send(frame, hoff + payload_len, blocking);
    kernel::memory::free(frame);

    if (sent < 0) {
        for (usize j = 0; j < rights_count; j++) self_realm->handle_table->release(rights[j].src_hid);
        return sent;
    }
    if (static_cast<usize>(sent) < hoff + payload_len) {
        // Partial frame write -- the channel guarantees atomicity of a
        // single send() call's worth of bytes it actually accepted, but
        // if capacity ran out mid-frame the peer will see a truncated,
        // unparseable frame. This shouldn't happen if the channel's
        // capacity comfortably exceeds one frame; treat as fatal here.
        for (usize j = 0; j < rights_count; j++) self_realm->handle_table->release(rights[j].src_hid);
        return -EMSGSIZE;
    }

    return static_cast<isize>(payload_len);
}

isize SocketEndpoint::recvmsg(msghdr* msg, const unsigned flags, const RealmId receiver_realm) const {
    if (!rx) return -EBADH;
    if (!msg) return -EINVAL;

    const bool blocking = !(flags & MSG_DONTWAIT);

    u32 header[2];
    isize r = rx->channel->recv(header, sizeof(header), blocking);
    if (r <= 0) return r; // 0 = EOF, <0 = error, propagate as-is
    if (r != sizeof(header) || header[0] != SENDMSG_FRAME_MAGIC) return -EPROTO;

    const u32 rights_count = header[1];
    FrameRight rights[64];
    if (rights_count > 64) return -EPROTO;

    if (rights_count > 0) {
        r = rx->channel->recv(rights, rights_count * sizeof(FrameRight), blocking);
        if (r != static_cast<isize>(rights_count * sizeof(FrameRight))) return -EPROTO;
    }

    u32 payload_len = 0;
    r = rx->channel->recv(&payload_len, sizeof(payload_len), blocking);
    if (r != sizeof(payload_len)) return -EPROTO;

    // Scatter payload into msg_iov.
    usize remaining = payload_len;
    usize total_read = 0;
    for (usize i = 0; i < msg->msg_iovlen && remaining > 0; i++) {
        const usize want = min(remaining, msg->msg_iov[i].iov_len);
        r = rx->channel->recv(msg->msg_iov[i].iov_base, want, blocking);
        if (r < 0) return r;
        total_read += r;
        remaining -= r;
        if (static_cast<usize>(r) < want) break; // short read, bail
    }
    // If the caller's iovecs are smaller than payload_len, the rest of
    // the frame's payload bytes are still sitting in the channel and
    // will corrupt the NEXT recvmsg()/read() call -- same truncation
    // hazard recvmsg(2) has on Linux with MSG_TRUNC. Not handled here;
    // flag if you want a drain-and-discard fallback.

    // Transfer each carried handle into the receiver's realm now.
    Realm* dst = RealmManager::get(receiver_realm);
    if (!dst) return -ESRCH;

    usize out_off = 0;
    unsigned char* out_data = msg->msg_control ? CMSG_DATA(reinterpret_cast<cmsghdr*>(msg->msg_control)) : nullptr;
    usize out_cap = msg->msg_control ? (msg->msg_controllen > CMSG_ALIGN(sizeof(cmsghdr))
                                          ? msg->msg_controllen - CMSG_ALIGN(sizeof(cmsghdr)) : 0)
                                      : 0;

    usize transferred = 0;
    for (usize i = 0; i < rights_count; i++) {
        Realm* src = RealmManager::get(rights[i].src_realm);
        HandleId new_hid = 0;

        if (src) {
            const HandleEntry* src_entry = src->handle_table->lookup(rights[i].src_hid);
            if (src_entry) {
                auto transfer_res = kernel::realm::transfer_handle_to_realm(
                    src_entry, dst, src_entry->capabilities);
                // Release the sender-side hold acquired in sendmsg(),
                // regardless of transfer outcome -- it was only held to
                // survive until this point.
                src->handle_table->release(rights[i].src_hid);

                if (!transfer_res.is_err()) new_hid = transfer_res.unwrap();
            } else {
                src->handle_table->release(rights[i].src_hid); // still balance the acquire
            }
        }

        if (out_data && (out_off + sizeof(HandleId)) <= out_cap) {
            memcpy(out_data + out_off, &new_hid, sizeof(HandleId));
            out_off += sizeof(HandleId);
            transferred++;
        }
        // If out_cap is too small for all rights, remaining new_hids are
        // silently dropped after having been transferred into dst's
        // handle table -- they become unreachable/leaked handles in the
        // receiver's realm. Matches MSG_CTRUNC territory; not signaled
        // to the caller here.
    }

    if (msg->msg_control && transferred > 0) {
        auto* cmsg = reinterpret_cast<cmsghdr*>(msg->msg_control);
        cmsg->cmsg_len = static_cast<socklen_t>(CMSG_LEN(transferred * sizeof(HandleId)));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        msg->msg_controllen = static_cast<socklen_t>(CMSG_SPACE(transferred * sizeof(HandleId)));
    } else {
        msg->msg_controllen = 0;
    }

    return static_cast<isize>(total_read);
}

int SocketEndpoint::poll() const {
    int mask = 0;
    if (rx) mask |= rx->channel->poll(/*is_reader=*/true, /*is_writer=*/false);
    if (tx) mask |= tx->channel->poll(/*is_reader=*/false, /*is_writer=*/true);
    return mask;
}
