// battery_widget.c
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

#include <fflags.h>
#include <power.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <stella.h>
#include "../theme/theme.h"

/* -------------------------------------------------------------------------
 * Internal context (one per battery slot)
 * ------------------------------------------------------------------------- */

typedef struct battery_ctx {
    int             battery_index;
    stella_widget_t bar;
    stella_widget_t label;
    stella_timer_t *timer;
} battery_ctx_t;

/* -------------------------------------------------------------------------
 * Hardware read
 * ------------------------------------------------------------------------- */

static int battery_read_status(int index, battery_status_t *out) {
    char path[32];
    snprintf(path, sizeof(path), "/dev/bat%d", index);

    HANDLE hdl = open(path, O_RDONLY);
    if (hdl < 0) return -1;

    int rc = ioctl(hdl, IOCTL_BAT_GET_STATUS, out);
    close(hdl);

    return (rc < 0) ? -1 : 0;
}

/* -------------------------------------------------------------------------
 * Timer callback
 * ------------------------------------------------------------------------- */

static void battery_update_cb(stella_timer_t *timer, void *user_data) {
    (void)timer;
    battery_ctx_t *ctx = user_data;

    battery_status_t st;
    if (battery_read_status(ctx->battery_index, &st) < 0 || !st.present) {
        stella_label_update(ctx->label, "N/A");
        stella_bar_set_value(ctx->bar, 0);
        return;
    }

    int  percent  = (st.percent == 255) ? 0 : (int)st.percent;
    bool charging = (st.state & BAT_STATE_CHARGING) != 0;
    bool critical = (st.state & BAT_STATE_CRITICAL) != 0;

    stella_bar_set_value(ctx->bar, percent);

    char buf[32];
    if (charging)
        snprintf(buf, sizeof(buf), "%d%% ⚡", percent);
    else
        snprintf(buf, sizeof(buf), "%d%%", percent);
    stella_label_update(ctx->label, buf);

    stella_color_t col;
    if (critical || percent <= 10)
        col = VESPERA_COL(VESPERA_DUSK_RED);
    else if (percent <= 20)
        col = VESPERA_COL(VESPERA_WARN);
    else
        col = VESPERA_COL(VESPERA_GREEN);

    stella_bar_set_indicator_color(ctx->bar, col);
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

stella_widget_t battery_widget_create(stella_widget_t parent, int battery_index) {
    battery_ctx_t *ctx = malloc(sizeof(battery_ctx_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(*ctx));
    ctx->battery_index = battery_index;

    /* Row container */
    stella_widget_t cont = stella_container_create(parent);
    stella_widget_set_size(cont, STELLA_SIZE_CONTENT, STELLA_SIZE_CONTENT);
    stella_widget_set_bg_transp(cont);
    stella_widget_no_border(cont);
    stella_widget_flex_row(cont,
                           STELLA_FLEX_CENTER,
                           STELLA_FLEX_CENTER,
                           STELLA_FLEX_CENTER);
    stella_widget_set_pad_col(cont, 4);

    /* Level bar (28 × 10 px) */
    ctx->bar = stella_bar_create(cont, 28, 10);
    stella_bar_set_range(ctx->bar, 0, 100);
    stella_bar_set_track_color(ctx->bar, VESPERA_COL(VESPERA_BORDER));
    stella_bar_set_track_radius(ctx->bar, 2);
    stella_bar_set_indicator_color(ctx->bar, VESPERA_COL(VESPERA_GREEN));
    stella_bar_set_indicator_radius(ctx->bar, 2);

    /* Percentage label */
    ctx->label = stella_label_create(cont, "");
    stella_text_set_color(ctx->label, VESPERA_COL(VESPERA_TEXT_DIM));
    stella_text_set_font(ctx->label, STELLA_FONT_14);

    /* Periodic refresh — fire immediately to populate on creation */
    ctx->timer = stella_timer_create(battery_update_cb, 15000, ctx);
    stella_timer_fire_now(ctx->timer);

    return cont;
}

void battery_widget_destroy(battery_ctx_t *ctx) {
    if (!ctx) return;
    stella_timer_delete(ctx->timer);
    free(ctx);
}
