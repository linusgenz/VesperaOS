// topbar.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 27.05.26.
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

#include "topbar.h"

#include <fflags.h>
#include <power.h>
#include <stdio.h>
#include <stella.h>
#include <sys/ioctl.h>

#include "../theme/theme.h"
#include "../widgets/battery_widget.h"
#include "../widgets/clock_widget.h"


static stella_widget_t g_topbar = NULL;

static int battery_get_count(void) {
    HANDLE hdl = open("/dev/power", O_RDONLY);
    if (hdl < 0) return 0;

    uint32_t count = 0;
    if (ioctl(hdl, IOCTL_POWER_GET_COUNT, &count) < 0) count = 0;
    close(hdl);

    if (count > 3) count = 3;
    return (int)count;
}

stella_widget_t topbar_create(stella_widget_t screen) {
    /* --- Bar container -------------------------------------------------- */
    g_topbar = stella_container_create(screen);
    stella_widget_set_size(g_topbar, STELLA_SIZE_FULL, TOPBAR_H);
    stella_widget_align(g_topbar, STELLA_ALIGN_TOP_MID, 0, 0);
    stella_widget_set_bg(g_topbar, VESPERA_COL(VESPERA_BAR), STELLA_OPA_COVER);
    stella_widget_set_border_bottom(g_topbar, VESPERA_COL(VESPERA_BORDER), 1);
    stella_widget_set_radius(g_topbar, 0);
    stella_widget_set_pad_hor(g_topbar, 12);
    stella_widget_set_pad_ver(g_topbar, 0);
    stella_widget_flex_row(g_topbar,
                           STELLA_FLEX_SPACE_BETWEEN,
                           STELLA_FLEX_CENTER,
                           STELLA_FLEX_CENTER);

    /* --- Left: logo label ----------------------------------------------- */
    stella_widget_t logo = stella_label_create(g_topbar, "VESPERA");
    stella_text_set_color(logo, VESPERA_COL(VESPERA_DUSK_RED));
    stella_text_set_font(logo, STELLA_FONT_14);

    /* --- Centre: clock widget ------------------------------------------- */
    clock_widget_create(g_topbar);

    /* --- Right: system tray --------------------------------------------- */
    stella_widget_t tray = stella_container_create(g_topbar);
    stella_widget_set_size(tray, STELLA_SIZE_CONTENT, TOPBAR_H);
    stella_widget_flex_row(tray,
                           STELLA_FLEX_END,
                           STELLA_FLEX_CENTER,
                           STELLA_FLEX_CENTER);
    stella_widget_set_pad_col(tray, 6);

    int battery_count = battery_get_count();
    for (int i = 0; i < battery_count; i++) {
        battery_widget_create(tray, i);
    }

    return g_topbar;
}
