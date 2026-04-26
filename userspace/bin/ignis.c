// powerd.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 21.04.26.
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

#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sysstd.h>
#include <vbus.h>
#include <vespera/dev/power.h>
#include <vespera/handles.h>

#include "log_client.h"

#define BATTERY_DEV "/dev/battery0"
#define POWER_STATE_FILE "/run/power/battery"
#define POWER_STATE_TMP "/run/power/battery.tmp"
#define POLL_INTERVAL_S 10
#define POLL_TIMEOUT_MS 1000

#ifndef IOCTL_BATTERY_GET_STATUS
#define IOCTL_BATTERY_GET_STATUS 0x5000
#endif
#ifndef IOCTL_AC_GET_STATUS
#define IOCTL_AC_GET_STATUS 0x5001
#endif

typedef struct {
    int present;
    int charging;
    int critical;
    int capacity;
    int voltage_mv;
    int remaining_mwh;
    int full_capacity_mwh;
    int rate_mw;
    int ac_online;
} power_state_t;

static int64_t g_bat_handle = -1;
static volatile int g_running = 1;

static void on_sigterm(int sig) {
    (void)sig;
    g_running = 0;
}

static void open_battery_dev(void) {
    if (g_bat_handle >= 0) return;
    int64_t h = sys_open((uint64_t)BATTERY_DEV, 0x02, 0, 0, 0, 0);
    if (h >= 0) {
        g_bat_handle = h;
        LOG_INFO("opened " BATTERY_DEV);
    }
}

static int read_power_state(power_state_t* out) {
    if (g_bat_handle < 0) return -1;

    vbus_battery_t bat = {0};
    int64_t r = sys_ioctl((uint64_t)g_bat_handle, IOCTL_BATTERY_GET_STATUS, (uint64_t)&bat, 0, 0, 0);
    if (r < 0) return (int)r;

    out->present = bat.present;
    out->charging = bat.charging;
    out->critical = bat.critical;
    out->capacity = bat.percent;
    out->remaining_mwh = (int)bat.remaining_mwh;
    out->full_capacity_mwh = (int)bat.full_capacity_mwh;
    out->rate_mw = (int)bat.rate_mw;

    vbus_ac_t ac = {0};
    r = sys_ioctl((uint64_t)g_bat_handle, IOCTL_AC_GET_STATUS, (uint64_t)&ac, 0, 0, 0);
    out->ac_online = (r >= 0) ? ac.online : -1;

    return 0;
}

static void write_state(const power_state_t* s) {
    FILE* f = fopen(POWER_STATE_TMP, "w");
    if (!f) return;
    fprintf(f, "capacity=%d\n", s->capacity);
    fprintf(f, "charging=%d\n", s->charging);
    fprintf(f, "present=%d\n", s->present);
    fprintf(f, "critical=%d\n", s->critical);
    fprintf(f, "voltage_mv=%d\n", s->voltage_mv);
    fprintf(f, "remaining_mwh=%d\n", s->remaining_mwh);
    fprintf(f, "full_capacity_mwh=%d\n", s->full_capacity_mwh);
    fprintf(f, "rate_mw=%d\n", s->rate_mw);
    fprintf(f, "ac_online=%d\n", s->ac_online);
    fclose(f);
    rename(POWER_STATE_TMP, POWER_STATE_FILE);
}

int main(void) {
    log_client_init("ignis");
    signal(SIGTERM, on_sigterm);

    LOG_INFO("powerd starting");

    sys_mkdir((uint64_t)"/run", 0, 0, 0, 0, 0);
    sys_mkdir((uint64_t)"/run/power", 0, 0, 0, 0, 0);

    vbus_subscribe("power", "");

    open_battery_dev();

    struct pollhdl hdls[1];
    hdls[0].hdl = HANDLE_VBUS;
    hdls[0].events = POLLIN;

    unsigned tick = 0;
    power_state_t state = {0};

    if (g_bat_handle >= 0 && read_power_state(&state) == 0) {
        write_state(&state);
    }

    while (g_running) {
        hdls[0].revents = 0;
        poll(hdls, 1, POLL_TIMEOUT_MS);

        int force_read = 0;
        for (;;) {
            vbus_header_t hdr;
            int r = vbus_recv(&hdr, NULL, 0);
            if (r <= 0) break;
            if (strcmp(hdr.interface, "power") == 0) force_read = 1;
        }

        tick++;
        if (force_read || tick >= (unsigned)(POLL_INTERVAL_S * (1000 / POLL_TIMEOUT_MS))) {
            tick = 0;
            open_battery_dev();
            if (g_bat_handle >= 0) {
                if (read_power_state(&state) == 0) {
                    write_state(&state);
                } else {
                    LOG_WARN("read_power_state failed");
                }
            }
        }
    }

    LOG_INFO("powerd shutting down");
    if (g_bat_handle >= 0) sys_close((uint64_t)g_bat_handle, 0, 0, 0, 0, 0);
    return 0;
}
