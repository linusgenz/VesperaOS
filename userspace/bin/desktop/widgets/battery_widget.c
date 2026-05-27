// battery_widget.cpp
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

#include "battery_widget.h"
#include "../theme/theme.h"
#include <power.h>    /* dein vesplib power.h */

typedef struct {
    lv_obj_t *bar;       /* lv_bar für füllstand */
    lv_obj_t *label;     /* "72%" */
    lv_timer_t *timer;
} battery_ctx_t;

static battery_ctx_t g_bat = {0};

static void battery_update_cb(lv_timer_t *t) {
    (void)t;

    int percent = -1;
    bool charging = false;

    /* Passe an dein power.h-API an */
    struct vespera_battery_info binfo;
    if (power_get_battery_info(&binfo) == 0) {
        percent  = binfo.percent;
        charging = binfo.charging;
    }

    if (percent < 0) {
        lv_label_set_text(g_bat.label, "N/A");
        return;
    }

    lv_bar_set_value(g_bat.bar, percent, LV_ANIM_OFF);

    char buf[16];
    snprintf(buf, sizeof(buf), charging ? "%d%% ⚡" : "%d%%", percent);
    lv_label_set_text(g_bat.label, buf);

    /* Farbe je nach Stand */
    lv_color_t col;
    if      (percent > 50) col = VESPERA_COL(VESPERA_GREEN);
    else if (percent > 20) col = VESPERA_COL(VESPERA_WARN);
    else                   col = VESPERA_COL(VESPERA_DUSK_RED);

    lv_obj_set_style_bg_color(g_bat.bar, col, LV_PART_INDICATOR);
}

lv_obj_t *battery_widget_create(lv_obj_t *parent) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(cont, 4, 0);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    g_bat.bar = lv_bar_create(cont);
    lv_obj_set_size(g_bat.bar, 28, 10);
    lv_bar_set_range(g_bat.bar, 0, 100);
    lv_obj_set_style_bg_color(g_bat.bar,
                               VESPERA_COL(VESPERA_BORDER), LV_PART_MAIN);
    lv_obj_set_style_radius(g_bat.bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(g_bat.bar, 2, LV_PART_INDICATOR);

    g_bat.label = lv_label_create(cont);
    lv_obj_set_style_text_color(g_bat.label,
                                 VESPERA_COL(VESPERA_TEXT_DIM), 0);
    lv_obj_set_style_text_font(g_bat.label, &lv_font_montserrat_12, 0);

    battery_update_cb(NULL);
    g_bat.timer = lv_timer_create(battery_update_cb, 15000, NULL);
    return cont;
}