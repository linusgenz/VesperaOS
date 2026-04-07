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

#define VBUS_MAX_SUBSCRIPTIONS 256

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
    static i64 subscribe(u64 realm_id, Channel* rx_channel,
                         const char* interface, const char* member);


    static void unsubscribe_realm(u64 realm_id);

    static void emit(const char* interface, const char* member,
                     const void* payload, usize payload_len);

    // emit no payload (pure signal)
    static void emit_signal(const char* interface, const char* member) {
        emit(interface, member, nullptr, 0);
    }

private:
    struct Subscription {
        u64      realm_id;
        Channel* channel;
        char     interface[48];
        char     member[48];   // empty = wildcard
        bool     active;
    };

    static Subscription subs_[VBUS_MAX_SUBSCRIPTIONS];
    static int          sub_count_;
    static Spinlock     lock_;
    static u64          serial_;

    static bool matches(const Subscription& s, const char* iface, const char* member);
};

#endif  // VESPERAOS_VBUS_MANAGER_H
