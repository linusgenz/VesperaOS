// vbus_manager.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 03.04.26.
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
#ifndef VESPERAOS_VBUS_MANAGER_H
#define VESPERAOS_VBUS_MANAGER_H

#include <uapi/vespera/vbus.h>
#include <vespera/ipc/channel.h>
#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

constexpr int VBUS_MAX_SUBSCRIPTIONS = 256;

/** @brief Maximum number of in-flight CALL messages awaiting a RETURN. */
constexpr int VBUS_MAX_PENDING_CALLS = 64;

//  VBusManager
//
//  Kernel-side message broker. Drivers call emit() to broadcast events;
//  userspace calls sys_vbus_subscribe to register interest.
//
//  Each subscribed realm owns a private Channel at handle slot 3 (HANDLE_VBUS).
//  emit() iterates all matching subscriptions and Channel::send()s header+payload
//  into each realm's channel. poll(HANDLE_VBUS, POLLIN) then wakes the realm.

class VBusManager {
   public:
    static void init();

    // interface / member: filter; empty member = wildcard (all members)
    static i64 subscribe(u64 realm_id, Channel* rx_channel, const char* interface, const char* member);

    static void unsubscribe_realm(u64 realm_id);

    static void emit(const char* interface, const char* member, const void* payload, usize payload_len);

    // emit no payload (pure signal)
    static void emit_signal(const char* interface, const char* member, const void* payload, usize payload_len);

    /**
     * @brief Routes an userspace-originated message to its destination.
     *
     * Dispatches based on @p hdr->type:
     *
     *   - VBUS_MSG_SIGNAL: broadcast to all matching subscribers, excluding
     *     the sender's own realm (no echo).
     *   - VBUS_MSG_CALL:   delivered to the *first* matching subscriber that
     *     is not the sender.  The (serial, caller_channel) pair is stored in
     *     the pending-call table so the reply can be routed back later.
     *   - VBUS_MSG_RETURN / VBUS_MSG_ERROR: looked up in the pending-call
     *     table by reply_serial; the reply is written to the original
     *     caller's channel and the pending entry is freed.
     */
    [[nodiscard]] static i64 emit_from_realm(
        u64 caller_realm_id, Channel* caller_channel, vbus_header_t* hdr, const void* payload, usize payload_len
    );

   private:
    struct Subscription {
        u64 realm_id;
        Channel* channel;
        char interface[48];
        char member[48];  // empty = wildcard
        bool active;
    };

    /**
     * @brief In-flight CALL awaiting a matching RETURN or ERROR.
     */
    struct PendingCall {
        u64 serial;
        Channel* caller_channel;  ///< where to deliver the RETURN
        u64 caller_realm_id;      ///< used only for diagnostics / timeout pruning
        bool active;
    };

    static Subscription subs_[VBUS_MAX_SUBSCRIPTIONS];
    static int sub_count_;

    static PendingCall pending_[VBUS_MAX_PENDING_CALLS];

    static Spinlock lock_;
    static u64 serial_;

    static bool matches(const Subscription& s, const char* iface, const char* member);

    /**
     * @brief Writes header + payload to @p ch if enough space is available.
     *
     * @return true if the message was written, false if the channel was full.
     */
    [[nodiscard]] static bool deliver(Channel* ch, const vbus_header_t* hdr, const void* payload, usize payload_len);

    /**
     * @brief Records a new pending-call entry.
     *
     * @return 0 on success, -ENOMEM if the table is full.
     */
    [[nodiscard]] static i64 push_pending(u64 serial, Channel* caller_channel, u64 caller_realm_id);

    /**
     * @brief Pops the pending-call entry for @p reply_serial.
     *
     * Writes the matched entry into @p out and marks the slot inactive.
     *
     * @return true if an entry was found, false otherwise.
     */
    static bool pop_pending(u64 reply_serial, PendingCall& out);
};

#endif  // VESPERAOS_VBUS_MANAGER_H
