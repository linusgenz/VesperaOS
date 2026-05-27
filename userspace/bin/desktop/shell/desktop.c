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
#include "../theme/theme.h"
#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
 * Interner Zustand
 * ------------------------------------------------------------------------- */

static lv_obj_t *g_screen       = NULL;
static lv_obj_t *g_wallpaper    = NULL;   /* Basis-Hintergrund              */
static lv_obj_t *g_dusk_overlay = NULL;   /* Dusk-Gradient drüber           */
static lv_obj_t *g_content_area = NULL;   /* Bereich für App-Fenster        */
static lv_obj_t *g_icon_grid    = NULL;   /* Linke Icon-Spalte              */

static desktop_icon_t g_icons[DESKTOP_ICON_MAX];
static int            g_icon_count = 0;

/* -------------------------------------------------------------------------
 * Vorwärts-Deklarationen (intern)
 * ------------------------------------------------------------------------- */

static void wallpaper_build(lv_obj_t *screen);
static void content_area_build(lv_obj_t *screen);
static void icon_grid_build(void);
static void icon_btn_build(lv_obj_t *parent, int idx);
static void icon_click_cb(lv_event_t *e);

/* =========================================================================
 * Public API
 * ========================================================================= */

void desktop_create(lv_obj_t *screen) {
    g_screen = screen;
    g_icon_count = 0;

    /* Reihenfolge: Wallpaper → Content-Area → Icon-Grid
     * (LVGL rendert in Erstellungsreihenfolge — Wallpaper muss zuerst) */
    wallpaper_build(screen);
    content_area_build(screen);
    icon_grid_build();
}

lv_obj_t *desktop_get_content_area(void) {
    return g_content_area;
}

bool desktop_add_icon(const desktop_icon_t *icon) {
    if (!icon || g_icon_count >= DESKTOP_ICON_MAX) return false;

    g_icons[g_icon_count++] = *icon;

    /* Icon sofort ins Grid einhängen wenn es bereits existiert */
    if (g_icon_grid) {
        icon_btn_build(g_icon_grid, g_icon_count - 1);
    }

    return true;
}

void desktop_clear_icons(void) {
    g_icon_count = 0;
    if (g_icon_grid) {
        lv_obj_clean(g_icon_grid);
    }
}

/* =========================================================================
 * Wallpaper
 *
 * Zwei Ebenen:
 *   1. g_wallpaper    — solide Fläche in VESPERA_BG (#1e1e2e)
 *   2. g_dusk_overlay — LVGL-Gradient von transparent (oben)
 *                       nach VESPERA_DUSK_WINE (unten)
 *                       simuliert den Abenddämmerungs-Look
 * ========================================================================= */

static void wallpaper_build(lv_obj_t *screen) {
    /* --- Basis-Layer ---------------------------------------------------- */
    static lv_style_t style_wp;
    lv_style_init(&style_wp);
    lv_style_set_bg_color(&style_wp, VESPERA_COL(VESPERA_BG));
    lv_style_set_bg_opa(&style_wp, LV_OPA_COVER);
    lv_style_set_border_width(&style_wp, 0);
    lv_style_set_radius(&style_wp, 0);
    lv_style_set_pad_all(&style_wp, 0);

    g_wallpaper = lv_obj_create(screen);
    lv_obj_remove_style_all(g_wallpaper);
    lv_obj_add_style(g_wallpaper, &style_wp, 0);
    lv_obj_set_size(g_wallpaper, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(g_wallpaper, 0, 0);
    lv_obj_clear_flag(g_wallpaper, LV_OBJ_FLAG_SCROLLABLE);

    /* --- Dusk-Gradient-Overlay ------------------------------------------ */
    static lv_style_t style_dusk;
    lv_style_init(&style_dusk);

    /* Gradient: oben = transparent, unten = VESPERA_DUSK_WINE             */
    lv_style_set_bg_color(&style_dusk, VESPERA_COL(VESPERA_BG));
    lv_style_set_bg_grad_color(&style_dusk, VESPERA_COL(VESPERA_DUSK_WINE));
    lv_style_set_bg_grad_dir(&style_dusk, LV_GRAD_DIR_VER);
    /* opa_start=0: oben komplett transparent, opa=180: unten halbtransparent */
    lv_style_set_bg_main_stop(&style_dusk, 100);   /* Gradient startet bei 40% */
    lv_style_set_bg_grad_stop(&style_dusk, 255);   /* Endet am unteren Rand    */
    lv_style_set_bg_opa(&style_dusk, 180);
    lv_style_set_border_width(&style_dusk, 0);
    lv_style_set_radius(&style_dusk, 0);

    g_dusk_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(g_dusk_overlay);
    lv_obj_add_style(g_dusk_overlay, &style_dusk, 0);
    lv_obj_set_size(g_dusk_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(g_dusk_overlay, 0, 0);
    lv_obj_clear_flag(g_dusk_overlay, LV_OBJ_FLAG_SCROLLABLE);
}

/* =========================================================================
 * Content-Area
 *
 * Transparenter Container der genau den Bereich zwischen Topbar und
 * Taskbar ausfüllt. App-Fenster werden hier als Kind-Objekte platziert.
 * ========================================================================= */

static void content_area_build(lv_obj_t *screen) {
    int32_t scr_h = lv_display_get_vertical_resolution(lv_display_get_default());

    int32_t area_y = DESKTOP_TOPBAR_HEIGHT;
    int32_t area_h = scr_h - DESKTOP_TOPBAR_HEIGHT - DESKTOP_TASKBAR_HEIGHT;

    static lv_style_t style_content;
    lv_style_init(&style_content);
    lv_style_set_bg_opa(&style_content, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_content, 0);
    lv_style_set_radius(&style_content, 0);
    lv_style_set_pad_all(&style_content, 0);

    g_content_area = lv_obj_create(screen);
    lv_obj_remove_style_all(g_content_area);
    lv_obj_add_style(g_content_area, &style_content, 0);
    lv_obj_set_size(g_content_area, LV_PCT(100), area_h);
    lv_obj_set_pos(g_content_area, 0, area_y);
    lv_obj_clear_flag(g_content_area, LV_OBJ_FLAG_SCROLLABLE);
}

/* =========================================================================
 * Icon-Grid
 *
 * Schmale Spalte am linken Rand der Content-Area.
 * Jedes Icon: farbige Box (oder lv_img) + Label darunter.
 * ========================================================================= */

static void icon_grid_build(void) {
    if (!g_content_area) return;

    static lv_style_t style_grid;
    lv_style_init(&style_grid);
    lv_style_set_bg_opa(&style_grid, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_grid, 0);
    lv_style_set_radius(&style_grid, 0);
    lv_style_set_pad_top(&style_grid, 12);
    lv_style_set_pad_left(&style_grid, 8);
    lv_style_set_pad_row(&style_grid, 10);
    lv_style_set_pad_column(&style_grid, 0);

    g_icon_grid = lv_obj_create(g_content_area);
    lv_obj_remove_style_all(g_icon_grid);
    lv_obj_add_style(g_icon_grid, &style_grid, 0);
    lv_obj_set_size(g_icon_grid, DESKTOP_ICON_GRID_WIDTH, LV_PCT(100));
    lv_obj_set_pos(g_icon_grid, 0, 0);
    lv_obj_clear_flag(g_icon_grid, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(g_icon_grid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_icon_grid,
                           LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_CENTER,
                           LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < g_icon_count; i++) {
        icon_btn_build(g_icon_grid, i);
    }
}

/* -------------------------------------------------------------------------
 * Ein einzelnes Desktop-Icon bauen
 *
 * Struktur (von oben nach unten im Flex-Container):
 *   lv_obj (icon_wrap)          ← klickbarer Container
 *     └─ lv_obj  (icon_box)    ← farbige Box / Bild
 *     └─ lv_label (icon_label) ← Name darunter
 * ------------------------------------------------------------------------- */

/* Styles sind static damit LVGL sie dauerhaft referenzieren kann.
 * Pro Icon wird ein eigener Style für die Farbe gebraucht — wir nutzen
 * inline set_style_* auf dem Objekt selbst (kein shared style nötig). */

static void icon_btn_build(lv_obj_t *parent, int idx) {
    const desktop_icon_t *ic = &g_icons[idx];

    /* Wrapper — transparent, kein Border, Flex-Column */
    lv_obj_t *wrap = lv_obj_create(parent);
    lv_obj_remove_style_all(wrap);
    lv_obj_set_size(wrap, DESKTOP_ICON_GRID_WIDTH - 16, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wrap, 0, 0);
    lv_obj_set_style_pad_all(wrap, 0, 0);
    lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wrap,
                           LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_CENTER,
                           LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);

    /* Hover-Effekt: leichte Aufhellung */
    lv_obj_set_style_bg_color(wrap, VESPERA_COL(VESPERA_BLUE), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(wrap, 40, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(wrap, 6, 0);

    /* Klick-Handler */
    lv_obj_add_flag(wrap, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(wrap, icon_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)idx);

    /* Icon-Box (Fallback: farbige Box mit erstem Buchstaben) */
    if (ic->icon_src) {
        lv_obj_t *img = lv_image_create(wrap);
        lv_image_set_src(img, ic->icon_src);
        lv_obj_set_size(img, DESKTOP_ICON_SIZE, DESKTOP_ICON_SIZE);
        lv_obj_set_style_radius(img, 8, 0);
    } else {
        lv_obj_t *box = lv_obj_create(wrap);
        lv_obj_remove_style_all(box);
        lv_obj_set_size(box, DESKTOP_ICON_SIZE, DESKTOP_ICON_SIZE);
        lv_obj_set_style_bg_color(box, ic->icon_color, 0);
        lv_obj_set_style_bg_opa(box, 200, 0);
        lv_obj_set_style_border_color(box, ic->icon_color, 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_border_opa(box, 100, 0);
        lv_obj_set_style_radius(box, 8, 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

        /* Erster Buchstabe als Fallback-Glyph */
        char glyph[2] = { ic->label ? ic->label[0] : '?', '\0' };
        lv_obj_t *glyph_lbl = lv_label_create(box);
        lv_label_set_text(glyph_lbl, glyph);
        lv_obj_set_style_text_color(glyph_lbl, VESPERA_COL(VESPERA_TEXT), 0);
        lv_obj_set_style_text_font(glyph_lbl, &lv_font_montserrat_16, 0);
        lv_obj_center(glyph_lbl);
    }

    /* Label */
    lv_obj_t *lbl = lv_label_create(wrap);
    lv_label_set_text(lbl, ic->label ? ic->label : "");
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl, DESKTOP_ICON_GRID_WIDTH - 16);
    lv_obj_set_style_text_color(lbl, VESPERA_COL(VESPERA_TEXT), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(lbl, 3, 0);
}

/* -------------------------------------------------------------------------
 * Icon-Click-Callback
 * ------------------------------------------------------------------------- */

static void icon_click_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);

    if (idx < 0 || idx >= g_icon_count) return;
    if (g_icons[idx].on_launch) {
        g_icons[idx].on_launch();
    }
}
