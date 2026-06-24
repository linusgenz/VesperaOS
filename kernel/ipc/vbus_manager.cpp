// vbus_manager.cpp
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

#include <klib/string.h>
#include <vespera/ipc/vbus_manager.h>
#include <vespera/log.h>
#include <vespera_errno.h>

#include "realm/realm.h"
#include "vespera/realm/realm_manager.h"

VBusManager::Subscription VBusManager::subs_[VBUS_MAX_SUBSCRIPTIONS] = {};
int VBusManager::sub_count_ = 0;
VBusManager::PendingCall VBusManager::pending_[VBUS_MAX_PENDING_CALLS] = {};
Spinlock VBusManager::lock_;
u64 VBusManager::serial_ = 0;

void VBusManager::init() {
    lock_.init("vbus_lock");
}

i64 VBusManager::subscribe(u64 realm_id, ChannelEndpoint* rx_ep, const char* interface, const char* member) {
    if (!rx_ep || !interface) return -EINVAL;

    SpinlockGuard g(lock_);

    // Deduplicate: same realm+interface+member already registered?
    for (auto& s : subs_) {
        if (!s.active) continue;
        if (s.realm_id == realm_id && strcmp(s.interface, interface) == 0 &&
            strcmp(s.member, member ? member : "") == 0) {
            return SUCCESS_CODE;  // idempotent
        }
    }

    // Find a free slot
    for (auto& sub : subs_) {
        if (!sub.active) {
            sub.realm_id = realm_id;
            sub.ep = rx_ep;
            strncpy(sub.interface, interface, 47);
            sub.interface[47] = '\0';
            strncpy(sub.member, member ? member : "", 47);
            sub.member[47] = '\0';
            sub.active = true;
            sub_count_++;
            /*Log::debug(
                "[VBus] realm %llu subscribed to %s.%s", realm_id, interface, member && member[0] ? member : "*"
            );*/
            return SUCCESS_CODE;
        }
    }
    return -ENOMEM;
}

void VBusManager::unsubscribe_realm(u64 realm_id) {
    SpinlockGuard g(lock_);
    for (auto& sub : subs_) {
        if (sub.active && sub.realm_id == realm_id) {
            sub.active = false;
            sub.ep = nullptr;
            sub_count_--;
        }
    }
}

void VBusManager::cancel_pending_for_realm(u64 realm_id) {
    SpinlockGuard g(lock_);
    for (PendingCall& p : pending_) {
        if (p.active && p.caller_realm_id == realm_id) {
            p.active = false;
            p.caller_channel = nullptr;
        }
    }
}

bool VBusManager::matches(const Subscription& s, const char* iface, const char* member) {
    if (strcmp(s.interface, iface) != 0) return false;
    if (s.member[0] == '\0') return true;  // wildcard
    return strcmp(s.member, member) == 0;
}

void VBusManager::emit(const char* interface, const char* member, const void* payload, usize payload_len) {
    // Build header on the stack
    vbus_header_t hdr = {};
    hdr.magic = VBUS_MAGIC;
    hdr.type = VBUS_MSG_SIGNAL;
    hdr.header_size = sizeof(vbus_header_t);
    hdr.payload_size = static_cast<u32>(payload_len);
    hdr.serial = __sync_add_and_fetch(&serial_, 1);
    hdr.reply_serial = 0;
    strncpy(hdr.interface, interface, 47);
    hdr.interface[47] = '\0';
    strncpy(hdr.member, member, 47);
    hdr.member[47] = '\0';
    hdr.sender_id = 0;

    // Total message size: header + payload must fit as one atomic write.
    // Channel::send() writes as much as fits, so if the channel is too full
    // we log a warning and skip that subscriber (non-blocking, never deadlocks).
    const usize total = sizeof(vbus_header_t) + payload_len;

    SpinlockGuard g(lock_);

    for (auto& s : subs_) {
        if (!s.active || !s.ep) continue;
        if (!matches(s, interface, member)) continue;

        // Check space before writing so we never write a partial message
        if (s.ep->channel->free_space() < total) {
            Log::warning("[VBus] channel full for realm %llu (%s), dropping %s.%s", s.realm_id, RealmManager::get(s.realm_id)->name, interface, member);
            continue;
        }

        s.ep->channel->send(&hdr, sizeof(hdr));
        if (payload && payload_len > 0) {
            s.ep->channel->send(payload, payload_len);
        }
    }
}

bool VBusManager::deliver(Channel* ch, const vbus_header_t* hdr, const void* payload, usize payload_len) {
    const usize total = sizeof(vbus_header_t) + payload_len;
    if (ch->free_space() < total) return false;

    ch->send(hdr, sizeof(vbus_header_t));
    if (payload && payload_len > 0) ch->send(payload, payload_len);

    return true;
}

i64 VBusManager::push_pending(u64 serial, Channel* caller_channel, u64 caller_realm_id) {
    for (PendingCall& p : pending_) {
        if (p.active) continue;
        p.serial = serial;
        p.caller_channel = caller_channel;
        p.caller_realm_id = caller_realm_id;
        p.active = true;
        return SUCCESS_CODE;
    }
    return -ENOMEM;
}

bool VBusManager::pop_pending(u64 reply_serial, PendingCall& out) {
    for (PendingCall& p : pending_) {
        if (!p.active || p.serial != reply_serial) continue;
        out = p;
        p.active = false;
        p.caller_channel = nullptr;
        return true;
    }
    return false;
}

void VBusManager::emit_signal(const char* interface, const char* member, const void* payload, usize payload_len) {
    vbus_header_t hdr = {};
    hdr.magic = VBUS_MAGIC;
    hdr.type = VBUS_MSG_SIGNAL;
    hdr.header_size = sizeof(vbus_header_t);
    hdr.payload_size = static_cast<u32>(payload_len);
    hdr.serial = __sync_add_and_fetch(&serial_, 1);
    hdr.reply_serial = 0;
    strncpy(hdr.interface, interface, VBUS_NAME_MAX - 1);
    hdr.interface[VBUS_NAME_MAX - 1] = '\0';
    strncpy(hdr.member, member, VBUS_NAME_MAX - 1);
    hdr.member[VBUS_NAME_MAX - 1] = '\0';
    hdr.sender_id = 0;

    SpinlockGuard g(lock_);

    for (Subscription& s : subs_) {
        if (!s.active || !s.ep) continue;
        if (!matches(s, interface, member)) continue;

        if (!deliver(s.ep->channel, &hdr, payload, payload_len)) {
            Log::warning("[VBus] channel full for realm %llu (%s), dropping %s.%s", s.realm_id, RealmManager::get(s.realm_id)->name, interface, member);
        }
    }
}

i64 VBusManager::emit_from_realm(
    u64 caller_realm_id, ChannelEndpoint* caller_ep, vbus_header_t* hdr, const void* payload, usize payload_len
) {
    if (!hdr || hdr->magic != VBUS_MAGIC) return -EINVAL;

    SpinlockGuard g(lock_);

    // Set the real sender, in case someone tries to impersonate someone else.
    hdr->sender_id = caller_realm_id;

    if (hdr->type == VBUS_MSG_RETURN || hdr->type == VBUS_MSG_ERROR) {
        PendingCall pc{};
        if (!pop_pending(hdr->reply_serial, pc)) {
            // No matching CALL in flight — caller may have timed out.
            return -ENOENT;
        }

        if (!pc.caller_channel) {
            // The calling realm already exited; discard silently.
            return SUCCESS_CODE;
        }

        if (!deliver(pc.caller_channel, hdr, payload, payload_len)) {
            return -EWOULDBLOCK;
        }

        return SUCCESS_CODE;
    }

    if (hdr->type == VBUS_MSG_CALL) {
        for (Subscription& s : subs_) {
            if (!s.active || !s.ep->channel) continue;
            if (s.realm_id == caller_realm_id) continue;  // no self-delivery
            if (!matches(s, hdr->interface, hdr->member)) continue;

            if (!deliver(s.ep->channel, hdr, payload, payload_len)) {
                return -EWOULDBLOCK;
            }

            // Record the pending call so RETURN can find its way back.
            i64 rc = push_pending(hdr->serial, caller_ep->channel, caller_realm_id);
            if (rc < 0) {
                // Pending table full — the reply will be unroutable.
                Log::warning("[VBus] pending table full, CALL serial %llu will not get a reply", hdr->serial);
            }

            return SUCCESS_CODE;
        }

        return -ENOENT;
    }

    if (hdr->type == VBUS_MSG_SIGNAL) {
        for (Subscription& s : subs_) {
            if (!s.active || !s.ep->channel) continue;
            if (s.realm_id == caller_realm_id) continue;  // no echo

            if (hdr->dest_realm_id != 0 && s.realm_id != hdr->dest_realm_id) continue;

            if (!matches(s, hdr->interface, hdr->member)) continue;

            if (!deliver(s.ep->channel, hdr, payload, payload_len)) {
                Log::warning("[VBus] channel full for realm %llu (%s), dropping %s.%s", s.realm_id, RealmManager::get(s.realm_id)->name, hdr->interface, hdr->member);
            }
        }
        return SUCCESS_CODE;
    }

    return -EINVAL;
}
