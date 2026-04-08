// batmon.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 06.04.26.
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

// battery_monitor.c – example userspace program using VBus
// Shows how to subscribe to battery events and react to them.

#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <vbus.h>

int main(void) {
    // Subscribe to battery and AC events (one call each)
    vbus_subscribe(VBUS_IFACE_POWER, VBUS_SIG_BATTERY_CHANGED);
    vbus_subscribe(VBUS_IFACE_POWER, VBUS_SIG_AC_CHANGED);
    vbus_subscribe(VBUS_IFACE_POWER, VBUS_SIG_LID_CHANGED);

    printf("battery-monitor: listening for power events...\n");

    struct pollhdl fds[] = {
        { .hdl = HANDLE_VBUS, .events = POLLIN }
    };

    while (1) {
        // Block until a vbus event arrives (or forever if nothing subscribed)
        int n = poll(fds, 1, -1);
        if (n <= 0) continue;

        if (!(fds[0].revents & POLLIN)) continue;

        vbus_header_t hdr;

        if (strcmp(hdr.member, VBUS_SIG_BATTERY_CHANGED) == 0) {
            vbus_battery_t batt;
            if (vbus_recv_battery(&hdr, &batt) > 0) {
                printf("[bat%u] %u%%  %s%s  (%u mWh remaining)\n",
                    batt.index,
                    batt.percent,
                    batt.charging  ? "charging"    : "discharging",
                    batt.critical  ? " *** CRITICAL ***" : "",
                    batt.remaining_mwh);

                if (!batt.charging && batt.percent <= 15 && batt.percent != 255) {
                    printf("WARNING: battery low!\n");
                }
            }

        } else if (strcmp(hdr.member, VBUS_SIG_AC_CHANGED) == 0) {
            vbus_ac_t ac;
            if (vbus_recv_ac(&hdr, &ac) > 0) {
                printf("[AC] adapter %s\n", ac.online ? "connected" : "disconnected");
            }

        } else if (strcmp(hdr.member, VBUS_SIG_LID_CHANGED) == 0) {
            vbus_lid_t lid;
            if (vbus_recv(&hdr, &lid, sizeof(lid)) > 0) {
                printf("[LID] %s\n", lid.open ? "opened" : "closed");
            }

        } else {
            vbus_recv(&hdr, NULL, 0);
        }
    }
}