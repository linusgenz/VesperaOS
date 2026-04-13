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

VBusManager::Subscription VBusManager::subs_[VBUS_MAX_SUBSCRIPTIONS] = {};
int VBusManager::sub_count_ = 0;
Spinlock VBusManager::lock_;
u64 VBusManager::serial_ = 0;

void VBusManager::init() {
    lock_.init("vbus_lock");
}

i64 VBusManager::subscribe(u64 realm_id, Channel* rx_channel, const char* interface, const char* member) {
    if (!rx_channel || !interface) return -EINVAL;

    SpinlockGuard g(lock_);

    // Deduplicate: same realm+interface+member already registered?
    for (int i = 0; i < VBUS_MAX_SUBSCRIPTIONS; i++) {
        auto& s = subs_[i];
        if (!s.active) continue;
        if (s.realm_id == realm_id && strcmp(s.interface, interface) == 0 &&
            strcmp(s.member, member ? member : "") == 0) {
            return SUCCESS_CODE;  // idempotent
        }
    }

    // Find a free slot
    for (int i = 0; i < VBUS_MAX_SUBSCRIPTIONS; i++) {
        if (!subs_[i].active) {
            subs_[i].realm_id = realm_id;
            subs_[i].channel = rx_channel;
            strncpy(subs_[i].interface, interface, 47);
            subs_[i].interface[47] = '\0';
            strncpy(subs_[i].member, member ? member : "", 47);
            subs_[i].member[47] = '\0';
            subs_[i].active = true;
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
    for (int i = 0; i < VBUS_MAX_SUBSCRIPTIONS; i++) {
        if (subs_[i].active && subs_[i].realm_id == realm_id) {
            subs_[i].active = false;
            subs_[i].channel = nullptr;
            sub_count_--;
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
    strncpy(hdr.sender, "kernel", 31);
    hdr.sender[31] = '\0';

    // Total message size: header + payload must fit as one atomic write.
    // Channel::send() writes as much as fits, so if the channel is too full
    // we log a warning and skip that subscriber (non-blocking, never deadlocks).
    const usize total = sizeof(vbus_header_t) + payload_len;

    SpinlockGuard g(lock_);

    for (int i = 0; i < VBUS_MAX_SUBSCRIPTIONS; i++) {
        auto& s = subs_[i];
        if (!s.active || !s.channel) continue;
        if (!matches(s, interface, member)) continue;

        // Check space before writing so we never write a partial message
        if (s.channel->free_space() < total) {
            Log::warning("[VBus] channel full for realm %llu, dropping %s.%s", s.realm_id, interface, member);
            continue;
        }

        s.channel->send(&hdr, sizeof(hdr));
        if (payload && payload_len > 0) {
            s.channel->send(payload, payload_len);
        }
    }
}
