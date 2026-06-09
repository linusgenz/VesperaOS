// stella.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 26.05.26.
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

/**
 * @file stella.h
 * @brief Window Client Library for the VesperaOS Graphic Subsystem.
 *
 * Stella is the lightweight, native client-side library for applications
 * running under VesperaOS. It wraps the underlying LVGL graphics engine and
 * handles IPC with the display server (Crepusculum) via vbus.
 *
 * Callers must NOT include <lvgl/lvgl.h> directly. All types exposed here
 * are Stella-owned; the LVGL dependency is an implementation detail.
 */

#ifndef VESPERAOS_STELLA_LIB_H
#define VESPERAOS_STELLA_LIB_H

#include <stdbool.h>
#include <stdint.h>
#include <crepusculum_protocol.h>
#include <vbus.h>

#ifdef __cplusplus
extern "C" {


#endif

/* -------------------------------------------------------------------------
 * Opaque handle types
 * ------------------------------------------------------------------------- */

typedef struct stella_window stella_window_t;
typedef void* stella_widget_t;

typedef void (*stella_close_cb_t)(stella_window_t* win, void* user_data);

/* -------------------------------------------------------------------------
 * Window configuration
 * ------------------------------------------------------------------------- */

typedef struct stella_config {
    uint32_t width;
    uint32_t height;
    uint32_t flags;
    const char* title;
} stella_config_t;

/* -------------------------------------------------------------------------
 * Colour
 *
 * stella_color_t is an opaque 32-bit value.  Use stella_rgb() to construct
 * one; use the STELLA_COL() convenience macro with theme colour tokens.
 * ------------------------------------------------------------------------- */

typedef struct {
    uint32_t _raw;
} stella_color_t;

/** Construct a colour from 8-bit red, green, blue components. */
static inline stella_color_t stella_rgb(uint8_t r, uint8_t g, uint8_t b) {
    stella_color_t c;
    c._raw = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    return c;
}

/** Construct a colour from a packed 0xRRGGBB hex literal. */
static inline stella_color_t stella_hex(uint32_t hex) {
    stella_color_t c;
    c._raw = hex & 0x00FFFFFFu;
    return c;
}

/* -------------------------------------------------------------------------
 * Opacity
 * ------------------------------------------------------------------------- */

typedef uint8_t stella_opa_t;

#define STELLA_OPA_TRANSP ((stella_opa_t)0)
#define STELLA_OPA_10 ((stella_opa_t)25)
#define STELLA_OPA_20 ((stella_opa_t)51)
#define STELLA_OPA_30 ((stella_opa_t)76)
#define STELLA_OPA_40 ((stella_opa_t)102)
#define STELLA_OPA_50 ((stella_opa_t)127)
#define STELLA_OPA_60 ((stella_opa_t)153)
#define STELLA_OPA_70 ((stella_opa_t)178)
#define STELLA_OPA_80 ((stella_opa_t)204)
#define STELLA_OPA_90 ((stella_opa_t)229)
#define STELLA_OPA_COVER ((stella_opa_t)255)

/* -------------------------------------------------------------------------
 * Fonts
 *
 * Fonts are referenced by opaque pointer; concrete instances live in stella.c
 * and map 1:1 to LVGL built-in fonts.  Include additional sizes as needed.
 * ------------------------------------------------------------------------- */

typedef struct stella_font stella_font_t;

extern const stella_font_t* STELLA_FONT_10;
extern const stella_font_t* STELLA_FONT_12;
extern const stella_font_t* STELLA_FONT_14;
extern const stella_font_t* STELLA_FONT_16;
extern const stella_font_t* STELLA_FONT_20;
extern const stella_font_t* STELLA_FONT_20_MATH;
extern const stella_font_t* STELLA_FONT_24;

/* -------------------------------------------------------------------------
 * Size helpers
 * ------------------------------------------------------------------------- */

/** Fill parent in this axis (100 %). */
#define STELLA_SIZE_FULL (-1) /* maps to LV_PCT(100)      */
/** Shrink-wrap to content. */
#define STELLA_SIZE_CONTENT (-2) /* maps to LV_SIZE_CONTENT  */

/* -------------------------------------------------------------------------
 * Flex alignment
 * ------------------------------------------------------------------------- */

typedef enum {
    STELLA_FLEX_START = 0,
    STELLA_FLEX_END = 1,
    STELLA_FLEX_CENTER = 2,
    STELLA_FLEX_SPACE_EVENLY = 3,
    STELLA_FLEX_SPACE_AROUND = 4,
    STELLA_FLEX_SPACE_BETWEEN = 5,
} stella_flex_align_t;

/* -------------------------------------------------------------------------
 * Widget alignment
 * ------------------------------------------------------------------------- */

typedef enum {
    STELLA_ALIGN_DEFAULT = 0,

    STELLA_ALIGN_TOP_LEFT,
    STELLA_ALIGN_TOP_MID,
    STELLA_ALIGN_TOP_RIGHT,

    STELLA_ALIGN_BOTTOM_LEFT,
    STELLA_ALIGN_BOTTOM_MID,
    STELLA_ALIGN_BOTTOM_RIGHT,

    STELLA_ALIGN_LEFT_MID,
    STELLA_ALIGN_RIGHT_MID,
    STELLA_ALIGN_CENTER,

    STELLA_ALIGN_OUT_TOP_LEFT,
    STELLA_ALIGN_OUT_TOP_MID,
    STELLA_ALIGN_OUT_TOP_RIGHT,

    STELLA_ALIGN_OUT_BOTTOM_LEFT,
    STELLA_ALIGN_OUT_BOTTOM_MID,
    STELLA_ALIGN_OUT_BOTTOM_RIGHT,

    STELLA_ALIGN_OUT_LEFT_TOP,
    STELLA_ALIGN_OUT_LEFT_MID,
    STELLA_ALIGN_OUT_LEFT_BOTTOM,

    STELLA_ALIGN_OUT_RIGHT_TOP,
    STELLA_ALIGN_OUT_RIGHT_MID,
    STELLA_ALIGN_OUT_RIGHT_BOTTOM,
} stella_align_t;

/* -------------------------------------------------------------------------
 * Text alignment
 * ------------------------------------------------------------------------- */

typedef enum {
    STELLA_TEXT_ALIGN_AUTO = 0,
    STELLA_TEXT_ALIGN_LEFT = 1,
    STELLA_TEXT_ALIGN_CENTER = 2,
    STELLA_TEXT_ALIGN_RIGHT = 3,
} stella_text_align_t;

/* -------------------------------------------------------------------------
 * Event callback
 * ------------------------------------------------------------------------- */

typedef void (*stella_event_cb_t)(stella_widget_t widget, void* user_data);

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

/**
 * Initialise the Stella library (vbus + LVGL).
 * Must be called once before any other Stella function.
 * @return 0 on success, negative on error.
 */
int stella_init(void);

/**
 * Advance the LVGL tick counter and run the timer handler.
 * Call every ~5 ms from the main loop.
 */
void stella_tick(uint32_t ms);

/* =========================================================================
 * Window management
 * ========================================================================= */

stella_window_t* stella_window_create(const stella_config_t* config);
void stella_window_destroy(stella_window_t* win);
void stella_process_events(stella_window_t* win);

void stella_handle_event(stella_window_t* win, const vbus_header_t* hdr, const vbus_payload_t* payload);

void stella_window_on_close(stella_window_t* win, stella_close_cb_t cb, void* user_data);
bool stella_window_should_close(stella_window_t* win);

/** Returns the root LVGL screen object for this window. */
stella_widget_t stella_window_get_screen(stella_window_t* win);

/* =========================================================================
 * Display information
 * ========================================================================= */

int32_t stella_display_width(void);
int32_t stella_display_height(void);

/* =========================================================================
 * Widget creation
 * ========================================================================= */

/** Blank container: all default styles stripped, scrolling disabled. */
stella_widget_t stella_container_create(stella_widget_t parent);

/** Label with initial text (NULL → empty). */
stella_widget_t stella_label_create(stella_widget_t parent, const char* text);

/** Image widget scaled to w×h pixels. */
stella_widget_t stella_image_create(stella_widget_t parent, const void* src, int32_t w, int32_t h);

/**
 * Image widget loaded from a filesystem path at runtime.
 * Requires the Stella FS driver (registered automatically in stella_init()).
 *
 * @param path  Absolute VesperaOS path, e.g. "/usr/share/icons/myapp.png"
 * @param w     Display width in pixels  (STELLA_SIZE_CONTENT = natural size)
 * @param h     Display height in pixels (STELLA_SIZE_CONTENT = natural size)
 * @return Widget handle, or NULL if the path could not be resolved.
 */
stella_widget_t stella_image_create_from_path(stella_widget_t parent, const char* path, int32_t w, int32_t h);

/** Button with a centred text label. */
stella_widget_t stella_button_create(stella_widget_t parent, const char* text, int32_t w, int32_t h);

/* =========================================================================
 * Size / position / alignment
 * ========================================================================= */

void stella_widget_set_size(stella_widget_t w, int32_t width, int32_t height);
void stella_widget_set_width(stella_widget_t w, int32_t width);
void stella_widget_set_height(stella_widget_t w, int32_t height);
void stella_widget_set_pos(stella_widget_t w, int32_t x, int32_t y);
void stella_widget_align(stella_widget_t w, stella_align_t align, int32_t dx, int32_t dy);
void stella_widget_center(stella_widget_t w);

/* =========================================================================
 * Background
 * ========================================================================= */

void stella_widget_set_bg(stella_widget_t w, stella_color_t color, stella_opa_t opa);
void stella_widget_set_bg_transp(stella_widget_t w);

/**
 * Vertical gradient (top → bottom).
 *
 * @param main_stop  0–255: point where col_top starts fading (~100 = 40 % down).
 * @param grad_stop  0–255: point where col_bot is fully reached (255 = bottom edge).
 * @param opa        Overall opacity of the gradient layer.
 */
void stella_widget_set_vertical_gradient(
    stella_widget_t w, stella_color_t col_top, stella_color_t col_bot, uint8_t main_stop, uint8_t grad_stop,
    stella_opa_t opa
);

/* =========================================================================
 * Border
 * ========================================================================= */

void stella_widget_no_border(stella_widget_t w);
void stella_widget_set_border(stella_widget_t w, stella_color_t color, int32_t width, stella_opa_t opa);

/** Bottom-only separator border at full opacity — typical for topbar/panel. */
void stella_widget_set_border_bottom(stella_widget_t w, stella_color_t color, int32_t width);

/** Top-only separator border at full opacity — typical for taskbar/panel. */
void stella_widget_set_border_top(stella_widget_t w, stella_color_t color, int32_t width);

/* =========================================================================
 * Shape
 * ========================================================================= */

void stella_widget_set_radius(stella_widget_t w, int32_t r);

/* =========================================================================
 * Padding
 * ========================================================================= */

void stella_widget_set_pad_all(stella_widget_t w, int32_t p);
void stella_widget_set_pad_hor(stella_widget_t w, int32_t p);
void stella_widget_set_pad_ver(stella_widget_t w, int32_t p);
void stella_widget_set_pad_top(stella_widget_t w, int32_t p);
void stella_widget_set_pad_left(stella_widget_t w, int32_t p);
void stella_widget_set_pad_row(stella_widget_t w, int32_t p);
void stella_widget_set_pad_col(stella_widget_t w, int32_t p);

/* =========================================================================
 * Flex layout
 * ========================================================================= */

void stella_widget_flex_row(
    stella_widget_t w, stella_flex_align_t main_place, stella_flex_align_t cross_place, stella_flex_align_t track_place
);

void stella_widget_flex_col(
    stella_widget_t w, stella_flex_align_t main_place, stella_flex_align_t cross_place, stella_flex_align_t track_place
);

/* =========================================================================
 * Text / label styling
 * ========================================================================= */

void stella_text_set_color(stella_widget_t w, stella_color_t color);
void stella_text_set_font(stella_widget_t w, const stella_font_t* font);
void stella_text_set_align(stella_widget_t w, stella_text_align_t align);
void stella_text_set_pad_top(stella_widget_t w, int32_t p);

void stella_label_update(stella_widget_t lbl, const char* text);
void stella_label_set_long_dot(stella_widget_t lbl);

/* =========================================================================
 * State / hover
 * ========================================================================= */

/** Background shown on pointer hover / press (LVGL FOCUSED state). */
void stella_widget_set_hover_bg(stella_widget_t w, stella_color_t color, stella_opa_t opa);

/* =========================================================================
 * Flags / misc
 * ========================================================================= */

void stella_widget_no_scroll(stella_widget_t w);

/** Destroy all children of w without destroying w itself. */
void stella_widget_clean(stella_widget_t w);

/* =========================================================================
 * Events
 * ========================================================================= */

/** Attach a click handler; user_data is forwarded verbatim. */
void stella_widget_on_click(stella_widget_t w, stella_event_cb_t cb, void* user_data);

/* =========================================================================
 * Timers
 * ========================================================================= */

typedef struct stella_timer stella_timer_t;

/** Callback signature: user_data is whatever was passed to stella_timer_create(). */
typedef void (*stella_timer_cb_t)(stella_timer_t* timer, void* user_data);

/**
 * Create a repeating timer.
 * @param cb          Callback invoked on each expiry.
 * @param period_ms   Interval in milliseconds.
 * @param user_data   Forwarded verbatim to cb.
 */
stella_timer_t* stella_timer_create(stella_timer_cb_t cb, uint32_t period_ms, void* user_data);

/** Delete a timer.  Safe to call with NULL. */
void stella_timer_delete(stella_timer_t* timer);

/** Mark the timer as ready so it fires on the very next stella_tick() call. */
void stella_timer_fire_now(stella_timer_t* timer);

/**
 * Create a bar widget.
 * Default range: 0–100.  Default value: 0.
 */
stella_widget_t stella_bar_create(stella_widget_t parent, int32_t w, int32_t h);

/** Set the value range [min, max]. */
void stella_bar_set_range(stella_widget_t bar, int32_t min, int32_t max);

/** Set the current value (clamped to [min, max], no animation). */
void stella_bar_set_value(stella_widget_t bar, int32_t value);

/** Set the background (track) colour and radius. */
void stella_bar_set_track_color(stella_widget_t bar, stella_color_t color);
void stella_bar_set_track_radius(stella_widget_t bar, int32_t r);

/** Set the filled (indicator) colour and radius. */
void stella_bar_set_indicator_color(stella_widget_t bar, stella_color_t color);
void stella_bar_set_indicator_radius(stella_widget_t bar, int32_t r);

void stella_widget_delete(stella_widget_t w);

// Utility

long long now_ms(void);
void sleep_ms(long long ms);

#ifdef __cplusplus
}
#endif

#endif  // VESPERAOS_STELLA_LIB_H
