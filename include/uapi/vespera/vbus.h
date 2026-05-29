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
#ifndef UAPI_VESPERA_VBUS_H
#define UAPI_VESPERA_VBUS_H

#include <vespera/types.h>

//  Layout on the channel:  [ vbus_header_t ][ payload_size bytes ]

#define VBUS_MAGIC 0x56425553UL

#define VBUS_MAX_PAYLOAD_SIZE 4096

/** @brief Maximum length of interface / member strings including NUL. */
#define VBUS_NAME_MAX 48

// Message types
#define VBUS_MSG_SIGNAL 0x01  // One-way broadcast, no reply
#define VBUS_MSG_CALL 0x02    // Method call (reply expected)
#define VBUS_MSG_RETURN 0x03  // Reply to a CALL
#define VBUS_MSG_ERROR 0x04

#define VBUS_IFACE_POWER "vespera.power"
#define VBUS_IFACE_DISPLAY "vespera.display"
#define VBUS_IFACE_INPUT "vespera.input"
#define VBUS_IFACE_STORAGE "vespera.storage"

#define VBUS_SIG_BATTERY_CHANGED "BatteryChanged"  // payload: vbus_battery_t
#define VBUS_SIG_AC_CHANGED "AcChanged"            // payload: vbus_ac_t
#define VBUS_SIG_SLEEP_REQUEST "SleepRequest"      // payload: none
#define VBUS_SIG_WAKE "Wake"                       // payload: none
#define VBUS_SIG_LID_CHANGED "LidChanged"          // payload: vbus_lid_t

// Subscribe flags
#define VBUS_SUB_WILDCARD 0x01  // empty member = subscribe all members of interface

typedef struct vbus_header {
    u32 magic;  // VBUS_MAGIC
    u8 type;    // VBUS_MSG_*
    u8 flags;
    uint16_t header_size;  // sizeof(vbus_header_t) for forward compat
    u32 payload_size;      // bytes that follow this header
    u64 serial;            // monotonic counter, set by emitter
    u64 reply_serial;      // serial of the CALL being answered; 0 for signals
    uint64_t dest_realm_id; // 0 = broadcast, else unicast
    char interface[48];    // e.g. "vespera.power"
    char member[48];       // e.g. "BatteryChanged"
    uint32_t sender_id;       // 0 for kernel or realm id
} vbus_header_t;

typedef struct vbus_battery {
    u8 percent;             // 0–100; 255 = unknown (battery removed)
    u8 charging;            // 1 = charging, 0 = discharging
    u8 present;             // 1 = battery present
    u8 critical;            // 1 = critically low (≤5%)
    u32 remaining_mwh;      // remaining energy in mWh; 0xFFFFFFFF = unknown
    u32 rate_mw;            // current charge/discharge rate in mW; 0 = unknown
    u32 full_capacity_mwh;  // last full capacity for reference
    u8 index;               // battery index (bat0=0, bat1=1, ...)
    u8 _pad[3];
} vbus_battery_t;

typedef struct vbus_ac {
    u8 online;  // 1 = AC adapter plugged in
    u8 _pad[3];
} vbus_ac_t;

typedef struct vbus_lid {
    u8 open;  // 1 = lid open, 0 = lid closed
    u8 _pad[3];
} vbus_lid_t;

typedef struct vbus_subscribe_args {
    char interface[48];
    char member[48];  // empty string = subscribe all members
    u32 flags;        // VBUS_SUB_*
} vbus_subscribe_args_t;

#endif  // UAPI_VESPERA_VBUS_H