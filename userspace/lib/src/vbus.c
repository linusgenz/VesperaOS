// vbus.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 28.05.26.
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

#include <errno.h>
#include <realm.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sysstd.h>
#include <vespera/handles.h>
#include <vespera/vbus.h>
static uint64_t g_local_serial = 0;

uint64_t vbus_next_serial(void) {
    return __sync_add_and_fetch(&g_local_serial, 1);
}

int vbus_subscribe(const char* interface, const char* member) {
    vbus_subscribe_args_t args = {};

    strncpy(args.interface, interface, 47);
    args.interface[47] = '\0';

    if (member) {
        strncpy(args.member, member, 47);
        args.member[47] = '\0';
    }

    return (int)sys_vbus_subscribe((uint64_t)&args, 0, 0, 0, 0, 0);
}

/**
 * @brief Unsubscribe from all vbus signals.
 */
int vbus_unsubscribe(void) {
    return (int)sys_vbus_unsubscribe(0, 0, 0, 0, 0, 0);
}

int vbus_emit_raw(vbus_header_t* hdr, const void* payload, uint32_t payload_size) {
    if (!hdr) return -EINVAL;

    hdr->magic = VBUS_MAGIC;
    hdr->header_size = sizeof(vbus_header_t);
    hdr->payload_size = (payload && payload_size > 0) ? payload_size : 0;

    // we do not have to try to fix the sender_id field, as the kernel will override it anyway to the correct value

    return sys_vbus_emit((uint64_t)hdr, (uint64_t)payload, hdr->payload_size, 0, 0, 0);
}

int64_t vbus_call(
    const char* interface, const char* member, RealmID sender_id, const void* payload, uint64_t payload_len,
    uint64_t* out_serial
) {
    vbus_header_t hdr = {0};

    hdr.magic = VBUS_MAGIC;
    hdr.type = VBUS_MSG_CALL;
    hdr.header_size = sizeof(vbus_header_t);
    hdr.payload_size = (uint32_t)payload_len;
    hdr.serial = vbus_next_serial();
    hdr.dest_realm_id = 0;
    hdr.reply_serial = 0;

    strncpy(hdr.interface, interface, VBUS_NAME_MAX - 1);
    hdr.interface[VBUS_NAME_MAX - 1] = '\0';
    strncpy(hdr.member, member, VBUS_NAME_MAX - 1);
    hdr.member[VBUS_NAME_MAX - 1] = '\0';
    hdr.sender_id = sender_id;

    if (out_serial) *out_serial = hdr.serial;

    return vbus_emit_raw(&hdr, payload, payload_len);
}

int64_t vbus_reply(const vbus_header_t* req_hdr, RealmID sender_id, const void* payload, uint64_t payload_len) {
    vbus_header_t hdr = {0};

    hdr.magic = VBUS_MAGIC;
    hdr.type = VBUS_MSG_RETURN;
    hdr.header_size = sizeof(vbus_header_t);
    hdr.payload_size = (uint32_t)payload_len;
    hdr.serial = vbus_next_serial();
    hdr.dest_realm_id = 0;
    hdr.reply_serial = req_hdr->serial;

    strncpy(hdr.interface, req_hdr->interface, VBUS_NAME_MAX - 1);
    hdr.interface[VBUS_NAME_MAX - 1] = '\0';
    strncpy(hdr.member, req_hdr->member, VBUS_NAME_MAX - 1);
    hdr.member[VBUS_NAME_MAX - 1] = '\0';
    hdr.sender_id = sender_id;

    return vbus_emit_raw(&hdr, payload, payload_len);
}

int64_t vbus_signal(
    const char* interface, const char* member, RealmID sender_id, const void* payload, uint64_t payload_len
) {
    vbus_header_t hdr = {0};
    hdr.magic = VBUS_MAGIC;
    hdr.type = VBUS_MSG_SIGNAL;
    hdr.header_size = sizeof(vbus_header_t);
    hdr.payload_size = (uint32_t)payload_len;
    hdr.serial = vbus_next_serial();
    hdr.dest_realm_id = 0;
    hdr.reply_serial = 0;

    strncpy(hdr.interface, interface, sizeof(hdr.interface) - 1);
    if (member) strncpy(hdr.member, member, sizeof(hdr.member) - 1);

    hdr.sender_id = sender_id;

    int64_t r = vbus_emit_raw(&hdr, payload, payload_len);
    return (r < 0) ? r : (int64_t)hdr.serial;
}

int64_t vbus_signal_to(
    const char* interface, const char* member, const RealmID sender_id, const RealmID dest_realm_id,
    const void* payload, uint64_t payload_len
) {
    vbus_header_t hdr = {0};
    hdr.magic = VBUS_MAGIC;
    hdr.type = VBUS_MSG_SIGNAL;
    hdr.header_size = sizeof(vbus_header_t);
    hdr.payload_size = (uint32_t)payload_len;
    hdr.serial = vbus_next_serial();
    hdr.dest_realm_id = dest_realm_id;  // 0 → broadcast
    hdr.sender_id = sender_id;

    strncpy(hdr.interface, interface, VBUS_NAME_MAX - 1);
    strncpy(hdr.member, member, VBUS_NAME_MAX - 1);

    int64_t r = vbus_emit_raw(&hdr, payload, payload_len);
    return (r < 0) ? r : (int64_t)hdr.serial;
}

int vbus_recv(vbus_header_t* hdr, void* payload_buf, size_t payload_cap) {
    if (!hdr) return -EINVAL;

    // Read header
    int r = (int)sys_channel_recv(HANDLE_VBUS, (uint64_t)hdr, sizeof(vbus_header_t), 0, 0, 0);
    if (r < 0) return r;                                 // -EAGAIN if empty
    if ((uint32_t)r < sizeof(vbus_header_t)) return -EIO;

    // Validate magic
    if (hdr->magic != VBUS_MAGIC) return -EIO;

    // Read payload
    if (hdr->payload_size > 0) {
        if (payload_buf && payload_cap >= hdr->payload_size) {
            sys_channel_recv(HANDLE_VBUS, (uint64_t)payload_buf, hdr->payload_size, 0, 0, 0);
        } else {
            // Drain payload into a small scratch buffer to keep the channel in sync
            uint8_t scratch[64];
            uint32_t remaining = hdr->payload_size;
            while (remaining > 0) {
                uint32_t chunk = remaining < sizeof(scratch) ? remaining : sizeof(scratch);
                int dr = (int)sys_channel_recv(HANDLE_VBUS, (uint64_t)scratch, chunk, 0, 0, 0);
                if (dr <= 0) break;
                remaining -= (uint32_t)dr;
            }
        }
    }
    return 1;
}

int vbus_recv_battery(vbus_header_t* hdr, vbus_battery_t* out) {
    return vbus_recv(hdr, out, sizeof(vbus_battery_t));
}

int vbus_recv_ac(vbus_header_t* hdr, vbus_ac_t* out) {
    return vbus_recv(hdr, out, sizeof(vbus_ac_t));
}