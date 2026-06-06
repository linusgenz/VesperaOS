// desktop.c
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

#include "desktop.h"

#include <realm.h>
#include <stdint.h>
#include <stdio.h>
#include <stella.h>
#include <string.h>

#include "../theme/theme.h"
#include "astra.h"

static stella_widget_t g_screen = NULL;
static stella_widget_t g_wallpaper = NULL;    /* Solid base layer          */
static stella_widget_t g_dusk_overlay = NULL; /* Dusk gradient on top      */
static stella_widget_t g_content_area = NULL; /* App-window container      */
static stella_widget_t g_icon_grid = NULL;    /* Left icon column          */

static desktop_icon_t g_icons[DESKTOP_ICON_MAX];
static int g_icon_count = 0;

/* -------------------------------------------------------------------------
 * Forward declarations
 * ------------------------------------------------------------------------- */

static void wallpaper_build(stella_widget_t screen);
static void content_area_build(stella_widget_t screen);
static void icon_grid_build(void);
static void icon_btn_build(stella_widget_t parent, int idx);
static void icon_click_cb(stella_widget_t widget, void *user_data);

void desktop_create(stella_widget_t screen) {
    g_screen = screen;
    g_icon_count = 0;

    desktop_astra_load();

    /* Render order: wallpaper → content area → icon grid */
    wallpaper_build(screen);
    content_area_build(screen);
    icon_grid_build();
}

stella_widget_t desktop_get_content_area(void) {
    return g_content_area;
}

bool desktop_add_icon(const desktop_icon_t *icon) {
    if (!icon || g_icon_count >= DESKTOP_ICON_MAX) return false;

    g_icons[g_icon_count++] = *icon;

    if (g_icon_grid) {
        icon_btn_build(g_icon_grid, g_icon_count - 1);
    }

    return true;
}

void desktop_clear_icons(void) {
    g_icon_count = 0;
    if (g_icon_grid) {
        stella_widget_clean(g_icon_grid);
    }
}

static void wallpaper_build(stella_widget_t screen) {
    /* --- Base layer ----------------------------------------------------- */
    g_wallpaper = stella_container_create(screen);
    stella_widget_set_size(g_wallpaper, STELLA_SIZE_FULL, STELLA_SIZE_FULL);
    stella_widget_set_pos(g_wallpaper, 0, 0);
    stella_widget_set_bg(g_wallpaper, VESPERA_COL(VESPERA_BG), STELLA_OPA_COVER);
    stella_widget_no_border(g_wallpaper);
    stella_widget_set_radius(g_wallpaper, 0);
    stella_widget_set_pad_all(g_wallpaper, 0);

    /* --- Dusk gradient overlay ------------------------------------------ */
    g_dusk_overlay = stella_container_create(screen);
    stella_widget_set_size(g_dusk_overlay, STELLA_SIZE_FULL, STELLA_SIZE_FULL);
    stella_widget_set_pos(g_dusk_overlay, 0, 0);
    stella_widget_set_vertical_gradient(
        g_dusk_overlay,
        VESPERA_COL(VESPERA_BG),        /* top colour                       */
        VESPERA_COL(VESPERA_DUSK_WINE), /* bottom colour                    */
        100,                            /* gradient begins at ~40 % height  */
        255,                            /* fully blended at bottom edge     */
        180                             /* overall opacity                  */
    );
    stella_widget_no_border(g_dusk_overlay);
    stella_widget_set_radius(g_dusk_overlay, 0);
}

/* =========================================================================
 * Content area
 *
 * Transparent container that fills the space between topbar and taskbar.
 * App windows are placed here as children.
 * ========================================================================= */

static void content_area_build(stella_widget_t screen) {
    int32_t scr_h = stella_display_height();
    int32_t area_y = DESKTOP_TOPBAR_HEIGHT;
    int32_t area_h = scr_h - DESKTOP_TOPBAR_HEIGHT - DESKTOP_TASKBAR_HEIGHT;

    g_content_area = stella_container_create(screen);
    stella_widget_set_size(g_content_area, STELLA_SIZE_FULL, area_h);
    stella_widget_set_pos(g_content_area, 0, area_y);
    stella_widget_set_bg_transp(g_content_area);
    stella_widget_no_border(g_content_area);
    stella_widget_set_radius(g_content_area, 0);
    stella_widget_set_pad_all(g_content_area, 0);
}

/* =========================================================================
 * Icon grid
 *
 * Narrow column on the left edge of the content area.
 * Each icon: coloured box (or image) + label below.
 * ========================================================================= */

static void icon_grid_build(void) {
    if (!g_content_area) return;

    g_icon_grid = stella_container_create(g_content_area);
    stella_widget_set_size(g_icon_grid, DESKTOP_ICON_GRID_WIDTH, STELLA_SIZE_FULL);
    stella_widget_set_pos(g_icon_grid, 0, 0);
    stella_widget_set_bg_transp(g_icon_grid);
    stella_widget_no_border(g_icon_grid);
    stella_widget_set_radius(g_icon_grid, 0);
    stella_widget_set_pad_top(g_icon_grid, 12);
    stella_widget_set_pad_left(g_icon_grid, 8);
    stella_widget_set_pad_row(g_icon_grid, 10);
    stella_widget_set_pad_col(g_icon_grid, 0);
    stella_widget_flex_col(g_icon_grid, STELLA_FLEX_START, STELLA_FLEX_CENTER, STELLA_FLEX_CENTER);

    for (int i = 0; i < g_icon_count; i++) {
        icon_btn_build(g_icon_grid, i);
    }
}

/* -------------------------------------------------------------------------
 * Single desktop icon
 *
 * Layout (flex column):
 *   container (wrap)  ← clickable, hover highlight
 *     ├─ image        ← real icon image  -or-
 *     │  container    ← fallback coloured box
 *     │    └─ label   ← first-letter glyph
 *     └─ label        ← icon name
 * ------------------------------------------------------------------------- */

static void icon_btn_build(stella_widget_t parent, int idx) {
    const desktop_icon_t *ic = &g_icons[idx];

    /* Clickable wrapper */
    stella_widget_t wrap = stella_container_create(parent);
    stella_widget_set_size(wrap, DESKTOP_ICON_GRID_WIDTH - 16, STELLA_SIZE_CONTENT);
    stella_widget_set_bg_transp(wrap);
    stella_widget_no_border(wrap);
    stella_widget_set_pad_all(wrap, 0);
    stella_widget_set_radius(wrap, 6);
    stella_widget_flex_col(wrap, STELLA_FLEX_START, STELLA_FLEX_CENTER, STELLA_FLEX_CENTER);
    stella_widget_set_hover_bg(wrap, VESPERA_COL(VESPERA_BLUE), 40);
    stella_widget_on_click(wrap, icon_click_cb, (void *)(intptr_t)idx);

    /* Icon visual */
    if (ic->icon_src) {
        stella_widget_t img =
            stella_image_create_from_path(wrap, (const char *)ic->icon_src, DESKTOP_ICON_SIZE, DESKTOP_ICON_SIZE);
        if (img) {
            stella_widget_set_radius(img, 8);
        } else {
            goto fallback_box;
        }
    } else {
    fallback_box:;
        /* Coloured fallback box with first-letter glyph */
        stella_widget_t box = stella_container_create(wrap);
        stella_widget_set_size(box, DESKTOP_ICON_SIZE, DESKTOP_ICON_SIZE);
        stella_widget_set_bg(box, ic->icon_color, 200);
        stella_widget_set_border(box, ic->icon_color, 1, 100);
        stella_widget_set_radius(box, 8);

        char glyph[2] = {ic->label ? ic->label[0] : '?', '\0'};
        stella_widget_t glyph_lbl = stella_label_create(box, glyph);
        stella_text_set_color(glyph_lbl, VESPERA_COL(VESPERA_TEXT));
        stella_text_set_font(glyph_lbl, STELLA_FONT_16);
        stella_widget_center(glyph_lbl);
    }

    /* Name label */
    stella_widget_t lbl = stella_label_create(wrap, ic->label ? ic->label : "");
    stella_label_set_long_dot(lbl);
    stella_widget_set_width(lbl, DESKTOP_ICON_GRID_WIDTH - 16);
    stella_text_set_color(lbl, VESPERA_COL(VESPERA_TEXT));
    stella_text_set_font(lbl, STELLA_FONT_10);
    stella_text_set_align(lbl, STELLA_TEXT_ALIGN_CENTER);
    stella_widget_set_pad_top(lbl, 3);
}

static void icon_click_cb(stella_widget_t widget, void *user_data) {
    (void)widget;
    int idx = (int)(intptr_t)user_data;
    if (idx < 0 || idx >= g_icon_count) return;
    if (g_icons[idx].on_launch) {
        g_icons[idx].on_launch(g_icons[idx].launch_data);
    }
}