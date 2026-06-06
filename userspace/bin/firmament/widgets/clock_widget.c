// clock_widget.c
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

#include <stella.h>
#include <time.h>
#include <stdio.h>

#include "../theme/theme.h"

/* -------------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------------- */

static stella_widget_t  g_clock_label = NULL;
static stella_timer_t  *g_clock_timer = NULL;

static const char *wday[] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
static const char *mon[]  = { "Jan","Feb","Mar","Apr","May","Jun",
                               "Jul","Aug","Sep","Oct","Nov","Dec" };

/* -------------------------------------------------------------------------
 * Timer callback
 * ------------------------------------------------------------------------- */

static void clock_update_cb(stella_timer_t *timer, void *user_data) {
    (void)timer;
    (void)user_data;

    if (!g_clock_label) return;

    time_t now = time(NULL);
    if (now < 0) return;

    struct tm *t = gmtime(&now);
    if (!t) return;

    char buf[64];
    snprintf(buf, sizeof(buf), "%s, %02d %s  %02d:%02d",
             wday[t->tm_wday], t->tm_mday, mon[t->tm_mon],
             t->tm_hour, t->tm_min);

    stella_label_update(g_clock_label, buf);
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

stella_widget_t clock_widget_create(stella_widget_t parent) {
    g_clock_label = stella_label_create(parent, "");
    stella_text_set_color(g_clock_label, VESPERA_COL(VESPERA_TEXT));
    stella_text_set_font(g_clock_label, STELLA_FONT_14);

    /* Populate immediately, then refresh every 30 s. */
    clock_update_cb(NULL, NULL);
    g_clock_timer = stella_timer_create(clock_update_cb, 30000, NULL);

    return g_clock_label;
}

void clock_widget_destroy(void) {
    stella_timer_delete(g_clock_timer);
    g_clock_timer = NULL;
    g_clock_label = NULL;
}
