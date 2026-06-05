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

#include <errno.h>
#include <realm.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sysstd.h>
#include <vespera/vbus.h>

#define HANDLE_VBUS ((int64_t)(0x4000000000000003ULL))  // HANDLE_TYPE_CHANNEL | 3

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Returns the next monotonically increasing message serial.
 *
 * The counter is process-local and starts at 1.  It is not guaranteed to be
 * unique across processes.
 */
uint64_t vbus_next_serial(void);

/**
 * @brief Subscribes the calling process to messages matching (interface, member).
 *
 * After this call, matching messages are queued into HANDLE_VBUS.
 * Use poll(HANDLE_VBUS, POLLIN) to wait for incoming messages.
 *
 * Pass member as NULL or "" to receive every member on interface (wildcard).
 *
 * @return 0 on success, negative errno on failure.
 */
int vbus_subscribe(const char* interface, const char* member);

/**
 * @brief Removes all VBus subscriptions for the calling process.
 *
 * @return 0 on success, negative errno on failure.
 */
int vbus_unsubscribe(void);

/**
 * @brief Sends a VBus message (header + payload) atomically in a single write operation.
 *
 * The caller must pre-populate the ‘interface’, ‘member’, and ‘type’ fields in the header object.
 *
 * @param hdr          Pointer to the partially initialized VBus header.
 * @param payload      Pointer to the payload data (or NULL if none is available).
 * @param payload_size Size of the payload in bytes (0 if NULL).
 * @return 0 on success, or a negative errno code on error.
 */
int vbus_emit_raw(vbus_header_t* hdr, const void* payload, uint32_t payload_size);

int64_t vbus_signal(
    const char* interface, const char* member, RealmID sender_id, const void* payload, uint64_t payload_len
);

/**
 * @brief Sends a VBUS_MSG_CALL and records its serial in out_serial.
 *
 * The caller must save *out_serial and match it against reply_serial in
 * incoming VBUS_MSG_RETURN messages to identify the response.
 *
 * @param interface    Target interface name; must be non-NULL.
 * @param member       Target method name; must be non-NULL.
 * @param sender_id    RealmID of the sender
 * @param payload      Call arguments; may be NULL.
 * @param payload_size Byte length of payload.
 * @param out_serial   Output: serial of the emitted CALL; must be non-NULL.
 *
 * @return 0 on success.
 * @return -EINVAL if interface, member, or out_serial is NULL.
 * @return Negative errno from vbus_emit_raw on failure.
 */
int64_t vbus_call(
    const char* interface, const char* member, RealmID sender_id, const void* payload, uint32_t payload_size,
    uint64_t* out_serial
);

/**
 * @brief Sends a VBUS_MSG_RETURN in reply to a received VBUS_MSG_CALL.
 *
 * Sets reply_serial = req->serial so the kernel routes the response back to
 * the original caller via the pending-call table.
 *
 * @param req          Header of the CALL being replied to; must be non-NULL.
 * @param sender_id    RealmID of the sender
 * @param payload      Reply data; may be NULL.
 * @param payload_size Byte length of payload.
 *
 * @return 0 on success.
 * @return -EINVAL if req is NULL.
 * @return Negative errno from vbus_emit_raw on failure.
 */
int64_t vbus_reply(const vbus_header_t* req, RealmID sender_id, const void* payload, uint32_t payload_size);

int64_t vbus_signal_to(
    const char* interface, const char* member, RealmID sender_id, RealmID dest_realm_id, const void* payload,
    uint64_t payload_len
);

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
int vbus_recv(vbus_header_t* hdr, void* payload_buf, size_t payload_cap);

/** @brief Convenience wrapper: reads a vbus_battery_t payload. */
int vbus_recv_battery(vbus_header_t* hdr, vbus_battery_t* out);

/** @brief Convenience wrapper: reads a vbus_ac_t payload. */
int vbus_recv_ac(vbus_header_t* hdr, vbus_ac_t* out);

#ifdef __cplusplus
}
#endif

#endif  // VESPLIB_VBUS_H
