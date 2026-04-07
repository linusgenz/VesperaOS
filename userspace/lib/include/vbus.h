// vbus.h
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
#ifndef VESPLIB_VBUS_H
#define VESPLIB_VBUS_H

#include <stddef.h>
#include <stdint.h>
#include <sysstd.h>
#include <vespera/vbus.h>

#define HANDLE_VBUS ((int64_t)(0x4000000000000003ULL))  // HANDLE_TYPE_CHANNEL | 3

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Subscribe to a vbus signal.
 *
 * After this call, matching signals emitted by the kernel (or other senders)
 * are written into HANDLE_VBUS.  Use poll(HANDLE_VBUS, POLLIN) to wait.
 *
 * @param interface  e.g. VBUS_IFACE_POWER
 * @param member     e.g. VBUS_SIG_BATTERY_CHANGED, or "" for all members
 * @return 0 on success, negative errno on failure
 */
static inline int vbus_subscribe(const char* interface, const char* member) {
    vbus_subscribe_args_t args = {};
    // safe string copy
    int i = 0;
    while (interface[i] && i < 47) {
        args.interface[i] = interface[i];
        i++;
    }
    i = 0;
    if (member) {
        while (member[i] && i < 47) {
            args.member[i] = member[i];
            i++;
        }
    }
    return (int)sys_vbus_subscribe((uint64_t)&args, 0, 0, 0, 0, 0);
}

/**
 * @brief Unsubscribe from all vbus signals.
 */
static inline int vbus_unsubscribe(void) {
    return (int)sys_vbus_unsubscribe(0, 0, 0, 0, 0, 0);
}

/**
 * @brief Read one complete vbus message (header + payload) from HANDLE_VBUS.
 *
 * Reads the header into *hdr.  If payload_buf is non-NULL and the message
 * has a payload that fits, it is also read.  Extra payload bytes are discarded.
 *
 * @param hdr         Output: filled with message header.
 * @param payload_buf Buffer for payload, or NULL to discard.
 * @param payload_cap Capacity of payload_buf in bytes.
 * @return 1 on success, 0 if channel empty (EAGAIN), <0 on error.
 */
static inline int vbus_recv(vbus_header_t* hdr, void* payload_buf, size_t payload_cap) {
    if (!hdr) return -22;  // EINVAL

    // Read header
    int r = (int)sys_channel_recv(HANDLE_VBUS, (uint64_t)hdr, sizeof(vbus_header_t), 0, 0, 0);
    if (r < 0) return r;                                 // -EAGAIN if empty
    if ((uint32_t)r < sizeof(vbus_header_t)) return -5;  // EIO, truncated

    // Validate magic
    if (hdr->magic != VBUS_MAGIC) return -5;

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

static inline int vbus_recv_battery(vbus_header_t* hdr, vbus_battery_t* out) {
    return vbus_recv(hdr, out, sizeof(vbus_battery_t));
}

static inline int vbus_recv_ac(vbus_header_t* hdr, vbus_ac_t* out) {
    return vbus_recv(hdr, out, sizeof(vbus_ac_t));
}

#ifdef __cplusplus
}
#endif

#endif  // VESPLIB_VBUS_H
