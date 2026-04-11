// statusbar.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 02.04.26.
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

#include "statusbar.h"

#include <power.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <vespera/fflags.h>

#include "termios.h"

#define MAX_BATTERIES 4

static char status_msg[STATUSBAR_MSG_MAX] = "VesperaOS ready";

static int term_width = 80;

static int get_term_width(void) {
    tty_size_t sz;

    if (tty_get_size(stdin, &sz) == 0 && sz.cols > 0) {
        return sz.cols;
    }

    return 80; // Fallback (wie bisher)
}

// Map percent + state to a Nerd Font battery glyph.
//   \uf244  battery-empty          (0 – 20 %)
//   \uf243  battery-quarter        (21 – 50 %)
//   \uf242  battery-half           (51 – 80 %)
//   \uf240  battery-full           (81 – 100 %)
//   \uf0e7  bolt  (charging – overrides percentage)
static const char* bat_icon(uint8_t pct, uint32_t state) {
    if (state & BAT_STATE_CHARGING) return "\uf0e7";
    if (pct > 100) return "\uf244";  // unknown → empty glyph
    if (pct <= 20) return "\uf244";
    if (pct <= 50) return "\uf243";
    if (pct <= 80) return "\uf242";
    return "\uf240";
}

static const char* bat_color(uint8_t pct, uint32_t state) {
    if (state & BAT_STATE_CHARGING) return "\033[38;2;97;214;113m";                // green
    if (pct > 100) return "\033[38;2;140;140;140m";                                // grey – unknown
    if ((state & BAT_STATE_CRITICAL) || pct <= 10) return "\033[38;2;235;77;77m";  // red
    if (pct <= 25) return "\033[38;2;245;174;63m";                                 // amber
    return "\033[38;2;220;220;220m";                                               // near-white
}

static int query_batteries(battery_status_t* bat_buf, int max) {
    HANDLE power_hdl = open("/dev/power", O_RDONLY);
    if (power_hdl < 0) return 0;

    uint32_t count = 0;
    if (ioctl(power_hdl, IOCTL_POWER_GET_COUNT, &count) < 0 || count == 0) {
        close(power_hdl);
        return 0;
    }
    close(power_hdl);

    if ((int)count > max) count = (uint32_t)max;

    int found = 0;
    char path[16];

    for (uint32_t i = 0; i < count; i++) {
        snprintf(path, sizeof(path), "/dev/bat%u", i);
        HANDLE hdl = open(path, O_RDONLY);
        if (hdl < 0) continue;

        battery_status_t* s = &bat_buf[found];
        if (ioctl(hdl, IOCTL_BAT_GET_STATUS, s) == 0 && s->present) {
            found++;
        }
        close(hdl);
    }

    return found;
}

// Colour palette (RGB)
#define C_BG "\033[48;2;22;22;36m"
#define C_ACCENT "\033[38;2;66;117;245m"
#define C_DIM "\033[38;2;120;120;140m"
#define C_RESET "\033[0m"
#define C_BG_RESET "\033[49m"

void statusbar_draw(void) {
    battery_status_t bats[MAX_BATTERIES];
    int n_bats = query_batteries(bats, MAX_BATTERIES);

    // each entry renders as:  <color><icon> NNN%<reset>
    char right_buf[512];
    char* rp = right_buf;
    int right_vis = 0;  // count of *visible* characters (no ANSI)

    if (n_bats > 0) {
        for (int i = 0; i < n_bats; i++) {
            battery_status_t* b = &bats[i];

            if (i > 0) {
                rp += snprintf(rp, sizeof(right_buf) - (rp - right_buf), C_DIM "  " C_RESET);
                right_vis += 2;
            }

            const char* color = bat_color(b->percent, b->state);
            const char* icon = bat_icon(b->percent, b->state);

            if (b->percent <= 100) {
                rp += snprintf(rp, sizeof(right_buf) - (rp - right_buf), "%s%s %3u%%" C_RESET, color, icon, b->percent);
            } else {
                rp += snprintf(rp, sizeof(right_buf) - (rp - right_buf), "%s%s  ?%%" C_RESET, color, icon);
            }
            right_vis += 6;
        }
    }
    *rp = '\0';

    // left: status message
    // Visible:  1 (icon) + 2 (spaces) + strlen(msg)
    char left_buf[STATUSBAR_MSG_MAX + 64];
    snprintf(
        left_buf,
        sizeof(left_buf),
        "\uf129" C_RESET
              "  "
              "\033[38;2;190;190;205m%s" C_RESET,
        status_msg
    );
    int left_vis = 1 + 2 + (int)strlen(status_msg);

    int padding = term_width - 2 - left_vis - right_vis;
    if (padding < 1) padding = 1;

    printf("\033[s");

    printf("\033[1;1H");
    printf("\033[2K");
    printf(" ");
    printf("%s", left_buf);
    for (int i = 0; i < padding; i++) printf(" ");
    if (n_bats > 0) printf("%s", right_buf);
    printf(" " C_RESET C_BG_RESET);

    printf("\033[2;1H");
    printf(C_ACCENT);
    for (int i = 0; i < term_width; i++) printf("\u2500");
    printf(C_RESET);

    printf("\033[u");
}

void statusbar_set_message(const char* msg) {
    if (msg) {
        strncpy(status_msg, msg, STATUSBAR_MSG_MAX - 1);
        status_msg[STATUSBAR_MSG_MAX - 1] = '\0';
    } else {
        strncpy(status_msg, "VesperaOS ready", STATUSBAR_MSG_MAX - 1);
    }
    statusbar_draw();
}

void statusbar_init(void) {
    term_width = get_term_width();
    printf("\033[3;r");
    printf("\033[3;1H");
    statusbar_draw();
}