// taskbar.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 03.06.26.
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

#include "taskbar.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stella.h>
#include <string.h>

#include "../theme/theme.h"

/* -------------------------------------------------------------------------
 * Internal constants
 * ------------------------------------------------------------------------- */

/* Horizontal gap between the launcher button and the window strip. */
#define LAUNCHER_STRIP_GAP 8

/* Horizontal padding on both sides of the bar. */
#define BAR_PAD_HOR 8

/* Gap between consecutive window buttons inside the strip. */
#define WIN_BTN_GAP 6

/* -------------------------------------------------------------------------
 * Internal types
 * ------------------------------------------------------------------------- */

typedef struct {
    taskbar_wid_t id;
    bool in_use;

    char title[64];
    char icon_src[128];
    stella_color_t icon_color;

    stella_widget_t btn; /* Root clickable container               */
    stella_widget_t lbl; /* Title label — updated by update_title  */

    void (*on_click)(void* data);
    void* click_data;
} taskbar_entry_t;

/* -------------------------------------------------------------------------
 * Module state
 * ------------------------------------------------------------------------- */

static stella_widget_t g_taskbar = NULL;
static stella_widget_t g_win_strip = NULL;

static taskbar_entry_t g_entries[TASKBAR_MAX_WINS];
static taskbar_wid_t g_focused_id = TASKBAR_WID_INVALID;

static void (*g_launcher_cb)(void* data) = NULL;
static void* g_launcher_data = NULL;

/* -------------------------------------------------------------------------
 * Forward declarations
 * ------------------------------------------------------------------------- */

static void launcher_btn_build(stella_widget_t bar);
static void win_strip_build(stella_widget_t bar, int32_t scr_w);
static void win_btn_build(taskbar_entry_t* e);
static void launcher_click_cb(stella_widget_t widget, void* user_data);
static void win_btn_click_cb(stella_widget_t widget, void* user_data);
static taskbar_entry_t* find_entry(taskbar_wid_t id);
static taskbar_entry_t* alloc_entry(void);

/* =========================================================================
 * Public API
 * ========================================================================= */

stella_widget_t taskbar_create(stella_widget_t screen) {
    int32_t scr_w = stella_display_width();

    memset(g_entries, 0, sizeof(g_entries));

    /* --- Bar container -------------------------------------------------- */
    g_taskbar = stella_container_create(screen);
    stella_widget_set_size(g_taskbar, STELLA_SIZE_FULL, TASKBAR_H);
    stella_widget_align(g_taskbar, STELLA_ALIGN_BOTTOM_MID, 0, 0);
    stella_widget_set_bg(g_taskbar, VESPERA_COL(VESPERA_BAR), STELLA_OPA_COVER);
    stella_widget_set_border_top(g_taskbar, VESPERA_COL(VESPERA_BORDER), 1);
    stella_widget_set_radius(g_taskbar, 0);
    stella_widget_set_pad_hor(g_taskbar, BAR_PAD_HOR);
    stella_widget_set_pad_ver(g_taskbar, 0);
    stella_widget_flex_row(g_taskbar, STELLA_FLEX_START, STELLA_FLEX_CENTER, STELLA_FLEX_CENTER);
    stella_widget_set_pad_col(g_taskbar, LAUNCHER_STRIP_GAP);

    /* --- Left: launcher button ------------------------------------------ */
    launcher_btn_build(g_taskbar);

    /* --- Centre/right: window button strip ------------------------------ */
    win_strip_build(g_taskbar, scr_w);

    return g_taskbar;
}

taskbar_wid_t taskbar_add_window(const taskbar_window_entry_t* entry) {
    if (!entry) return TASKBAR_WID_INVALID;

    taskbar_entry_t* e = alloc_entry();
    if (!e) return TASKBAR_WID_INVALID;

    e->id = entry->id;
    e->in_use = true;
    e->icon_color = entry->icon_color;
    e->on_click = entry->on_click;
    e->click_data = entry->click_data;
    e->btn = NULL;
    e->lbl = NULL;

    strlcpy(e->title, entry->title ? entry->title : "", sizeof(e->title));
    e->title[sizeof(e->title) - 1] = '\0';

    strlcpy(e->icon_src, entry->icon_src ? entry->icon_src : "", sizeof(e->icon_src));
    e->icon_src[sizeof(e->icon_src) - 1] = '\0';

    if (g_win_strip) {
        win_btn_build(e);
    }

    return e->id;
}

void taskbar_remove_window(taskbar_wid_t id) {
    if (id == TASKBAR_WID_INVALID) return;

    taskbar_entry_t* e = find_entry(id);
    if (!e) return;

    if (g_focused_id == id) {
        g_focused_id = TASKBAR_WID_INVALID;
    }

    if (e->btn) {
        stella_widget_delete(e->btn);
        e->btn = NULL;
        e->lbl = NULL;
    }

    e->in_use = false;
}

void taskbar_set_focused(taskbar_wid_t id) {
    /* Clear the previous focus highlight */
    if (g_focused_id != TASKBAR_WID_INVALID) {
        taskbar_entry_t* prev = find_entry(g_focused_id);
        if (prev && prev->btn) {
            stella_widget_set_bg(prev->btn, VESPERA_COL(VESPERA_BAR), 0);
            stella_widget_set_border(prev->btn, VESPERA_COL(VESPERA_BORDER), 1, 100);
        }
    }

    g_focused_id = id;

    taskbar_entry_t* e = find_entry(id);
    if (e && e->btn) {
        stella_widget_set_bg(e->btn, VESPERA_COL(VESPERA_BLUE), 55);
        stella_widget_set_border(e->btn, VESPERA_COL(VESPERA_BLUE), 1, 180);
    }
}

void taskbar_clear_focused(void) {
    if (g_focused_id == TASKBAR_WID_INVALID) return;

    taskbar_entry_t* e = find_entry(g_focused_id);
    if (e && e->btn) {
        stella_widget_set_bg(e->btn, VESPERA_COL(VESPERA_BAR), 0);
        stella_widget_set_border(e->btn, VESPERA_COL(VESPERA_BORDER), 1, 100);
    }

    g_focused_id = TASKBAR_WID_INVALID;
}

void taskbar_update_title(taskbar_wid_t id, const char* title) {
    taskbar_entry_t* e = find_entry(id);
    if (!e) return;

    strlcpy(e->title, title ? title : "", sizeof(e->title));
    e->title[sizeof(e->title) - 1] = '\0';

    if (e->lbl) {
        stella_label_update(e->lbl, e->title);
    }
}

void taskbar_set_launcher_cb(void (*cb)(void* data), void* data) {
    g_launcher_cb = cb;
    g_launcher_data = data;
}

/* =========================================================================
 * Internal – Launcher button
 *
 * Fixed-width button on the left side of the taskbar.
 * Fires g_launcher_cb when clicked.
 * ========================================================================= */

static void launcher_click_cb(stella_widget_t widget, void* user_data) {
    (void)widget;
    (void)user_data;
    if (g_launcher_cb) {
        g_launcher_cb(g_launcher_data);
    }
}

static void launcher_btn_build(stella_widget_t bar) {
    stella_widget_t btn = stella_container_create(bar);
    stella_widget_set_size(btn, TASKBAR_LAUNCHER_W, TASKBAR_BTN_H);
    stella_widget_set_bg(btn, VESPERA_COL(VESPERA_BLUE), 30);
    stella_widget_set_border(btn, VESPERA_COL(VESPERA_BLUE), 1, 120);
    stella_widget_set_radius(btn, 6);
    stella_widget_set_pad_all(btn, 0);
    stella_widget_set_hover_bg(btn, VESPERA_COL(VESPERA_BLUE), 65);
    stella_widget_on_click(btn, launcher_click_cb, NULL);

    stella_widget_t icon = stella_image_create_from_path(
        btn, "/usr/share/icons/launcher.svg", 16, 16
    );
    stella_widget_center(icon);
}

/* =========================================================================
 * Internal – Window button strip
 *
 * Horizontal flex container that fills the space to the right of the launcher.
 * Window buttons are added as children of this container.
 * ========================================================================= */

static void win_strip_build(stella_widget_t bar, int32_t scr_w) {
    /* Compute available width: screen - both-side bar padding
     * - launcher button - launcher-to-strip gap.                           */
    int32_t strip_w = scr_w - (BAR_PAD_HOR * 2) - TASKBAR_LAUNCHER_W - LAUNCHER_STRIP_GAP;

    g_win_strip = stella_container_create(bar);
    stella_widget_set_size(g_win_strip, strip_w, TASKBAR_H);
    stella_widget_set_bg_transp(g_win_strip);
    stella_widget_no_border(g_win_strip);
    stella_widget_set_radius(g_win_strip, 0);
    stella_widget_set_pad_hor(g_win_strip, 0);
    stella_widget_set_pad_ver(g_win_strip, 0);
    stella_widget_set_pad_col(g_win_strip, WIN_BTN_GAP);
    stella_widget_flex_row(g_win_strip, STELLA_FLEX_START, STELLA_FLEX_CENTER, STELLA_FLEX_CENTER);
}

/* =========================================================================
 * Internal – Single window button
 *
 * Layout (horizontal flex):
 *
 *   [btn container]  ← clickable, hover + focus highlight
 *     ├─ [icon_box]  ← colored square (or image if icon_src set)
 *     │    └─ [glyph_lbl]  ← first letter, only for fallback box
 *     └─ [title_lbl] ← truncated with "..."
 * ========================================================================= */

static void win_btn_click_cb(stella_widget_t widget, void* user_data) {
    (void)widget;
    taskbar_wid_t id = (taskbar_wid_t)(uintptr_t)user_data;
    taskbar_entry_t* e = find_entry(id);
    if (e && e->on_click) {
        e->on_click(e->click_data);
    }
}

static void win_btn_build(taskbar_entry_t* e) {
    /* --- Root container ------------------------------------------------- */
    stella_widget_t btn = stella_container_create(g_win_strip);
    stella_widget_set_size(btn, TASKBAR_BTN_W, TASKBAR_BTN_H);
    stella_widget_set_bg(btn, VESPERA_COL(VESPERA_BAR), 0);
    stella_widget_set_border(btn, VESPERA_COL(VESPERA_BORDER), 1, 100);
    stella_widget_set_radius(btn, 6);
    stella_widget_set_pad_hor(btn, 6);
    stella_widget_set_pad_ver(btn, 0);
    stella_widget_set_pad_col(btn, 5);
    stella_widget_set_hover_bg(btn, VESPERA_COL(VESPERA_BLUE), 35);
    stella_widget_flex_row(btn, STELLA_FLEX_START, STELLA_FLEX_CENTER, STELLA_FLEX_CENTER);
    stella_widget_on_click(btn, win_btn_click_cb, (void*)(uintptr_t)e->id);

    /* --- Icon ----------------------------------------------------------- */
    bool icon_loaded = false;

    if (e->icon_src[0] != '\0') {
        stella_widget_t img = stella_image_create_from_path(btn, e->icon_src, TASKBAR_ICON_SZ, TASKBAR_ICON_SZ);
        if (img) {
            stella_widget_set_radius(img, 4);
            icon_loaded = true;
        }
    }

    if (!icon_loaded) {
        /* Fallback: colored box with the window's first-letter glyph */
        stella_widget_t icon_box = stella_container_create(btn);
        stella_widget_set_size(icon_box, TASKBAR_ICON_SZ, TASKBAR_ICON_SZ);
        stella_widget_set_bg(icon_box, e->icon_color, 200);
        stella_widget_set_radius(icon_box, 4);
        stella_widget_no_border(icon_box);

        char glyph[2] = {e->title[0] ? e->title[0] : '?', '\0'};
        stella_widget_t glyph_lbl = stella_label_create(icon_box, glyph);
        stella_text_set_color(glyph_lbl, VESPERA_COL(VESPERA_TEXT));
        stella_text_set_font(glyph_lbl, STELLA_FONT_10);
        stella_widget_center(glyph_lbl);
    }

    /* --- Title label ---------------------------------------------------- */
    int32_t lbl_w = TASKBAR_BTN_W - TASKBAR_ICON_SZ - 5 /* pad_col */ - 12 /* pad_hor */;

    stella_widget_t lbl = stella_label_create(btn, e->title);
    stella_label_set_long_dot(lbl);
    stella_widget_set_width(lbl, lbl_w);
    stella_text_set_color(lbl, VESPERA_COL(VESPERA_TEXT));
    stella_text_set_font(lbl, STELLA_FONT_10);

    e->btn = btn;
    e->lbl = lbl;
}

void taskbar_set_minimized(taskbar_wid_t id, bool minimized) {
    taskbar_entry_t* e = find_entry(id);
    if (!e || !e->btn) return;

    if (minimized) {
        stella_widget_set_bg(e->btn, VESPERA_COL(VESPERA_BAR), 0);
        stella_widget_set_border(e->btn, VESPERA_COL(VESPERA_BORDER), 1, 40);
    }
    else {
        stella_widget_set_bg(e->btn, VESPERA_COL(VESPERA_BAR), 0);
        stella_widget_set_border(e->btn, VESPERA_COL(VESPERA_BORDER), 1, 100);
    }
}

/* =========================================================================
 * Helpers
 * ========================================================================= */

static taskbar_entry_t* find_entry(taskbar_wid_t id) {
    for (int i = 0; i < TASKBAR_MAX_WINS; i++) {
        if (g_entries[i].in_use && g_entries[i].id == id) {
            return &g_entries[i];
        }
    }
    return NULL;
}

static taskbar_entry_t* alloc_entry(void) {
    for (int i = 0; i < TASKBAR_MAX_WINS; i++) {
        if (!g_entries[i].in_use) {
            return &g_entries[i];
        }
    }
    return NULL;
}
