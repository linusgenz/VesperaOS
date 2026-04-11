// powermon.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 11.04.26.
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

#include "powermon.h"

#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sysstd.h>
#include <vbus.h>
#include <time.h>
#include "statusbar.h"

/** Periodic battery-poll interval (ms).  Catches slow % drift that ACPI
 *  does not emit events for (e.g. linear drain between _BST notifications).
 *  And even if it does, it’s not consistent or regular. */
#define POWER_MONITOR_POLL_MS 30000

static int64_t mono_ms(void) {
    timespec_t ts = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static void handle_battery_changed(const vbus_battery_t* bat) {
    statusbar_draw();

    if (bat->critical && !bat->charging) {
        statusbar_set_message("\uf244  Battery critical \xe2\x80\x94 please plug in");
    }
}

static void handle_ac_changed(const vbus_ac_t* ac) {
    if (ac->online) {
        statusbar_set_message("\uf1e6  AC adapter connected");
    } else {
        statusbar_set_message("\uf240  Running on battery");
    }
}

static void handle_lid_changed(const vbus_lid_t* lid) {
    if (lid->open) {
        statusbar_set_message("\uf831  Lid opened");
    }
}

void power_monitor_unit(void* arg) {
    (void)arg;

    vbus_subscribe(VBUS_IFACE_POWER, "");

    pollhdl_t ph = {
        .hdl = HANDLE_VBUS,
        .events = POLLIN,
        .revents = 0,
    };

    // Absolute time at which the next periodic redraw is due.
    int64_t next_poll_at = mono_ms() + POWER_MONITOR_POLL_MS;

    while (1) {
        int64_t now = mono_ms();
        int64_t timeout = next_poll_at - now;

        if (timeout < 1) timeout = 1;

        int r = poll(&ph, 1, (int)timeout);

        if (r > 0 && (ph.revents & POLLIN)) {
            vbus_header_t hdr;
            uint8_t payload[64];

            while (vbus_recv(&hdr, payload, sizeof(payload)) == 1) {
                if (strcmp(hdr.interface, VBUS_IFACE_POWER) != 0) continue;

                if (strcmp(hdr.member, VBUS_SIG_BATTERY_CHANGED) == 0) {
                    handle_battery_changed((const vbus_battery_t*)payload);

                } else if (strcmp(hdr.member, VBUS_SIG_AC_CHANGED) == 0) {
                    handle_ac_changed((const vbus_ac_t*)payload);

                } else if (strcmp(hdr.member, VBUS_SIG_LID_CHANGED) == 0) {
                    handle_lid_changed((const vbus_lid_t*)payload);
                }
            }

            ph.revents = 0;
        }

        if (now >= next_poll_at) {
            statusbar_draw();
            next_poll_at = now + POWER_MONITOR_POLL_MS;
        }
    }
}