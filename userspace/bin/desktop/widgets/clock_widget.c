// clock_widget.cpp
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

#include "clock_widget.h"
#include "../theme/theme.h"
#include <time.h>   /* dein vesplib time.h */

static lv_obj_t  *g_clock_label = NULL;
static lv_timer_t *g_clock_timer = NULL;

static void clock_update_cb(lv_timer_t *timer) {
    (void)timer;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm t;
    /* vesplib hat hoffentlich localtime_r oder du nutzt gmtime_r */
    gmtime_r(&ts.tv_sec, &t);

    char buf[32];
    /* "Di, 26. Mai  14:37" */
    static const char *wday[] = {"So","Mo","Di","Mi","Do","Fr","Sa"};
    static const char *mon[]  = {"Jan","Feb","Mär","Apr","Mai","Jun",
                                  "Jul","Aug","Sep","Okt","Nov","Dez"};
    snprintf(buf, sizeof(buf), "%s, %d. %s  %02d:%02d",
             wday[t.tm_wday], t.tm_mday, mon[t.tm_mon],
             t.tm_hour, t.tm_min);

    lv_label_set_text(g_clock_label, buf);
}

lv_obj_t *clock_widget_create(lv_obj_t *parent) {
    g_clock_label = lv_label_create(parent);
    lv_obj_set_style_text_color(g_clock_label,
                                 VESPERA_COL(VESPERA_TEXT), 0);
    lv_obj_set_style_text_font(g_clock_label,
                                &lv_font_montserrat_14, 0);

    clock_update_cb(NULL);  /* Sofort befüllen */
    g_clock_timer = lv_timer_create(clock_update_cb, 30000, NULL); /* 30s reicht */
    return g_clock_label;
}

void clock_widget_destroy(void) {
    if (g_clock_timer) { lv_timer_delete(g_clock_timer); g_clock_timer = NULL; }
}