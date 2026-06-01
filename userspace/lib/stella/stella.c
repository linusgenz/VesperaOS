// stella.c
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

#include <crepusculum_protocol.h>
#include <fflags.h>
#include <lvgl/lvgl.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stella.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <vbus.h>
#include <vbus_display.h>
#include <vespera/handles.h>

static inline lv_color_t _col(stella_color_t c) {
    uint8_t r = (c._raw >> 16) & 0xFF;
    uint8_t g = (c._raw >> 8) & 0xFF;
    uint8_t b = c._raw & 0xFF;
    return lv_color_make(r, g, b);
}

static inline lv_opa_t _opa(stella_opa_t o) {
    return (lv_opa_t)o;
}

/* stella_font_t is just a thin wrapper so callers stay opaque. */
struct stella_font {
    const lv_font_t *lv;
};

static inline const lv_font_t *_font(const stella_font_t *f) {
    return f ? f->lv : &lv_font_montserrat_12;
}

/* Map STELLA_SIZE_FULL / STELLA_SIZE_CONTENT to LVGL equivalents. */
static inline int32_t _sz(int32_t v) {
    if (v == STELLA_SIZE_FULL) return LV_PCT(100);
    if (v == STELLA_SIZE_CONTENT) return LV_SIZE_CONTENT;
    return v;
}

/* =========================================================================
 * Font singletons
 * ========================================================================= */

static const stella_font_t _f10 = {&lv_font_montserrat_10};
static const stella_font_t _f12 = {&lv_font_montserrat_12};
static const stella_font_t _f14 = {&lv_font_montserrat_14};
static const stella_font_t _f16 = {&lv_font_montserrat_16};
static const stella_font_t _f20 = {&lv_font_montserrat_20};
static const stella_font_t _f24 = {&lv_font_montserrat_24};

const stella_font_t *STELLA_FONT_10 = &_f10;
const stella_font_t *STELLA_FONT_12 = &_f12;
const stella_font_t *STELLA_FONT_14 = &_f14;
const stella_font_t *STELLA_FONT_16 = &_f16;
const stella_font_t *STELLA_FONT_20 = &_f20;
const stella_font_t *STELLA_FONT_24 = &_f24;

/* =========================================================================
 * Window struct (private)
 * ========================================================================= */

struct stella_window {
    int32_t window_id;
    uint32_t width;
    uint32_t height;

    HANDLE sync_shm_fd;
    HANDLE fb_shm_fd;
    crep_sync_t *win_sync;
    uint32_t *win_pixels;

    lv_display_t *lv_disp;
    uint32_t *lv_fb_buffer;

    int32_t last_mouse_x;
    int32_t last_mouse_y;
    uint32_t last_buttons;

    bool should_close;
    stella_close_cb_t close_cb;
    void *close_user_data;

    lv_indev_t *lv_indev;
};

/* =========================================================================
 * Internal LVGL callbacks
 * ========================================================================= */

static void stella_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    stella_window_t *win = lv_display_get_user_data(disp);
    if (!win) return;

    uint32_t rect_w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t rect_h = (uint32_t)(area->y2 - area->y1 + 1);
    uint32_t *src = (uint32_t *)px_map;

    for (uint32_t y = 0; y < rect_h; y++) {
        uint32_t dst_y = (uint32_t)area->y1 + y;
        if (dst_y >= win->height) break;

        uint32_t dst_x = (uint32_t)area->x1;
        uint32_t copy_w = rect_w;
        if (dst_x + copy_w > win->width) copy_w = win->width - dst_x;

        memcpy(&win->win_pixels[dst_y * win->width + dst_x], &src[(size_t)(y * rect_w)], copy_w * sizeof(uint32_t));
    }

    __atomic_store_n(&win->win_sync->dirty, 1u, __ATOMIC_RELEASE);

    vbus_display_commit_t payload = {.window_id = (uint32_t)win->window_id};
    vbus_signal(VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_COMMIT, get_realm_id(), &payload, sizeof(payload));

    lv_display_flush_ready(disp);
}

static void stella_mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    stella_window_t *win = lv_indev_get_user_data(indev);
    if (!win) return;

    data->point.x = win->last_mouse_x;
    data->point.y = win->last_mouse_y;
    data->state = (win->last_buttons & (1u << 0)) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* =========================================================================
 * Window management
 * ========================================================================= */

stella_window_t *stella_window_create(const stella_config_t *config) {
    stella_window_t *win = malloc(sizeof(stella_window_t));
    if (!win) return NULL;
    memset(win, 0, sizeof(stella_window_t));

    vbus_display_create_window_t req = {0};
    req.width = config->width;
    req.height = config->height;
    req.flags = config->flags;
    strncpy(req.title, config->title ? config->title : "Stella App", sizeof(req.title) - 1);

    uint64_t my_serial = 0;
    if (vbus_call(VBUS_IFACE_DISPLAY, VBUS_DISP_CREATE_WINDOW, get_realm_id(), &req, sizeof(req), &my_serial) < 0) {
        free(win);
        return NULL;
    }

    /* Wait for the synchronous window-info reply. */
    vbus_header_t resp_hdr;
    vbus_display_window_info_t resp;
    while (1) {
        struct pollhdl pfd = {.hdl = HANDLE_VBUS, .events = POLLIN};
        poll(&pfd, 1, -1);
        if (vbus_recv(&resp_hdr, &resp, sizeof(resp)) == 1) {
            if (resp_hdr.type == VBUS_MSG_RETURN && resp_hdr.reply_serial == my_serial) break;
        }
    }

    if (resp.status < 0) {
        free(win);
        return NULL;
    }

    /* SHM mapping */
    win->sync_shm_fd = shm_open(resp.sync_shm, O_RDWR, 0666);
    win->win_sync =
        (crep_sync_t *)mmap(NULL, sizeof(crep_sync_t), PROT_READ | PROT_WRITE, MAP_SHARED, win->sync_shm_fd, 0);

    win->fb_shm_fd = shm_open(resp.fb_shm, O_RDWR, 0666);
    const size_t fb_size = (size_t)(resp.width * resp.height) * sizeof(uint32_t);
    win->win_pixels = (uint32_t *)mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, win->fb_shm_fd, 0);

    win->window_id = (int32_t)resp.window_id;
    win->width = resp.width;
    win->height = resp.height;

    /* LVGL display */
    win->lv_disp = lv_display_create((int32_t)win->width, (int32_t)win->height);
    lv_display_set_user_data(win->lv_disp, win);

    uint32_t buf_px = win->width * (win->height / 10);
    win->lv_fb_buffer = malloc(buf_px * sizeof(uint32_t));
    lv_display_set_buffers(
        win->lv_disp, win->lv_fb_buffer, NULL, buf_px * sizeof(uint32_t), LV_DISPLAY_RENDER_MODE_PARTIAL
    );
    lv_display_set_flush_cb(win->lv_disp, stella_flush_cb);

    /* LVGL input device */
    win->lv_indev = lv_indev_create();
    lv_indev_set_type(win->lv_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(win->lv_indev, win->lv_disp);
    lv_indev_set_user_data(win->lv_indev, win);
    lv_indev_set_read_cb(win->lv_indev, stella_mouse_read_cb);

    return win;
}

void stella_window_destroy(stella_window_t *win) {
    if (!win) return;
    if (win->win_pixels) munmap(win->win_pixels, (size_t)(win->width * win->height) * sizeof(uint32_t));
    if (win->win_sync) munmap(win->win_sync, sizeof(crep_sync_t));
    if (win->fb_shm_fd != INVALID_HANDLE) close(win->fb_shm_fd);
    if (win->sync_shm_fd != INVALID_HANDLE) close(win->sync_shm_fd);
    free(win->lv_fb_buffer);
    free(win);
}

void stella_window_resize(stella_window_t *win, uint32_t new_w, uint32_t new_h) {
    if (!win) return;
    if (win->width == new_w && win->height == new_h) return;

    if (win->win_pixels && win->win_pixels != MAP_FAILED) {
        munmap(win->win_pixels, (size_t)(win->width * win->height) * sizeof(uint32_t));
        win->win_pixels = NULL;
    }

    if (win->lv_fb_buffer) {
        free(win->lv_fb_buffer);
    }
    win->lv_fb_buffer = NULL;

    win->width = new_w;
    win->height = new_h;

    const size_t fb_size = (size_t)(new_w * new_h) * sizeof(uint32_t);
    win->win_pixels = (uint32_t *)mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, win->fb_shm_fd, 0);
    if (win->win_pixels == MAP_FAILED) {
        printf("stella: window_resize mmap failed (%ux%u) errno: %u\n", new_w, new_h, errno);
        win->win_pixels = NULL;
        return;
    }

    lv_display_set_resolution(win->lv_disp, (int32_t)new_w, (int32_t)new_h);

    uint32_t buf_px = new_w * (new_h / 10);
    win->lv_fb_buffer = malloc(buf_px * sizeof(uint32_t));
    lv_display_set_buffers(
        win->lv_disp, win->lv_fb_buffer, NULL, buf_px * sizeof(uint32_t), LV_DISPLAY_RENDER_MODE_PARTIAL
    );

    lv_obj_invalidate(lv_display_get_screen_active(win->lv_disp));
}

void stella_process_events(stella_window_t *win) {
    vbus_header_t hdr;
    union {
        vbus_display_input_event_t input;
        vbus_display_configure_t configure;
    } payload;

    while (vbus_recv(&hdr, &payload, sizeof(payload)) == 1) {
        if (strcmp(hdr.member, VBUS_DISP_INPUT_EVENT) == 0 && payload.input.window_id == (uint32_t)win->window_id) {
            win->last_mouse_x = payload.input.local_x;
            win->last_mouse_y = payload.input.local_y;
            win->last_buttons = payload.input.buttons;
        } else if (strcmp(hdr.member, VBUS_DISP_WINDOW_CONFIGURE) == 0 &&
                   payload.configure.window_id == (uint32_t)win->window_id) {
            stella_window_resize(win, payload.configure.width, payload.configure.height);
        }
    }
}

stella_widget_t stella_window_get_screen(stella_window_t *win) {
    if (!win || !win->lv_disp) return NULL;
    return (stella_widget_t)lv_display_get_screen_active(win->lv_disp);
}

/* =========================================================================
 * Display information
 * ========================================================================= */

int32_t stella_display_width(void) {
    return lv_display_get_horizontal_resolution(lv_display_get_default());
}

int32_t stella_display_height(void) {
    return lv_display_get_vertical_resolution(lv_display_get_default());
}

/* =========================================================================
 * Widget creation
 * ========================================================================= */

stella_widget_t stella_container_create(stella_widget_t parent) {
    lv_obj_t *obj = lv_obj_create((lv_obj_t *)parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return (stella_widget_t)obj;
}

stella_widget_t stella_label_create(stella_widget_t parent, const char *text) {
    lv_obj_t *lbl = lv_label_create((lv_obj_t *)parent);
    lv_label_set_text(lbl, text ? text : "");
    return (stella_widget_t)lbl;
}

stella_widget_t stella_image_create(stella_widget_t parent, const void *src, int32_t w, int32_t h) {
    lv_obj_t *img = lv_image_create((lv_obj_t *)parent);
    lv_image_set_src(img, src);
    lv_obj_set_size(img, w, h);
    return (stella_widget_t)img;
}

stella_widget_t stella_button_create(stella_widget_t parent, const char *text, int32_t w, int32_t h) {
    lv_obj_t *btn = lv_button_create((lv_obj_t *)parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text ? text : "");
    lv_obj_center(lbl);
    return (stella_widget_t)btn;
}

/* =========================================================================
 * Size / position / alignment
 * ========================================================================= */

void stella_widget_set_size(stella_widget_t w, int32_t width, int32_t height) {
    lv_obj_set_size((lv_obj_t *)w, _sz(width), _sz(height));
}

void stella_widget_set_width(stella_widget_t w, int32_t width) {
    lv_obj_set_width((lv_obj_t *)w, _sz(width));
}

void stella_widget_set_height(stella_widget_t w, int32_t height) {
    lv_obj_set_height((lv_obj_t *)w, _sz(height));
}

void stella_widget_set_pos(stella_widget_t w, int32_t x, int32_t y) {
    lv_obj_set_pos((lv_obj_t *)w, x, y);
}

void stella_widget_align(stella_widget_t w, stella_align_t align, int32_t dx, int32_t dy) {
    /* stella_align_t values are defined to match lv_align_t ordinals. */
    lv_obj_align((lv_obj_t *)w, (lv_align_t)align, dx, dy);
}

void stella_widget_center(stella_widget_t w) {
    lv_obj_center((lv_obj_t *)w);
}

/* =========================================================================
 * Background
 * ========================================================================= */

void stella_widget_set_bg(stella_widget_t w, stella_color_t color, stella_opa_t opa) {
    lv_obj_set_style_bg_color((lv_obj_t *)w, _col(color), 0);
    lv_obj_set_style_bg_opa((lv_obj_t *)w, _opa(opa), 0);
}

void stella_widget_set_bg_transp(stella_widget_t w) {
    lv_obj_set_style_bg_opa((lv_obj_t *)w, LV_OPA_TRANSP, 0);
}

void stella_widget_set_vertical_gradient(
    stella_widget_t w, stella_color_t col_top, stella_color_t col_bot, uint8_t main_stop, uint8_t grad_stop,
    stella_opa_t opa
) {
    lv_obj_t *obj = (lv_obj_t *)w;
    lv_obj_set_style_bg_color(obj, _col(col_top), 0);
    lv_obj_set_style_bg_grad_color(obj, _col(col_bot), 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_stop(obj, main_stop, 0);
    lv_obj_set_style_bg_grad_stop(obj, grad_stop, 0);
    lv_obj_set_style_bg_opa(obj, _opa(opa), 0);
}

/* =========================================================================
 * Border
 * ========================================================================= */

void stella_widget_no_border(stella_widget_t w) {
    lv_obj_set_style_border_width((lv_obj_t *)w, 0, 0);
}

void stella_widget_set_border(stella_widget_t w, stella_color_t color, int32_t width, stella_opa_t opa) {
    lv_obj_set_style_border_color((lv_obj_t *)w, _col(color), 0);
    lv_obj_set_style_border_width((lv_obj_t *)w, width, 0);
    lv_obj_set_style_border_opa((lv_obj_t *)w, _opa(opa), 0);
}

void stella_widget_set_border_bottom(stella_widget_t w, stella_color_t color, int32_t width) {
    lv_obj_set_style_border_side((lv_obj_t *)w, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color((lv_obj_t *)w, _col(color), 0);
    lv_obj_set_style_border_width((lv_obj_t *)w, width, 0);
}

/* =========================================================================
 * Shape
 * ========================================================================= */

void stella_widget_set_radius(stella_widget_t w, int32_t r) {
    lv_obj_set_style_radius((lv_obj_t *)w, r, 0);
}

/* =========================================================================
 * Padding
 * ========================================================================= */

void stella_widget_set_pad_all(stella_widget_t w, int32_t p) {
    lv_obj_set_style_pad_all((lv_obj_t *)w, p, 0);
}

void stella_widget_set_pad_hor(stella_widget_t w, int32_t p) {
    lv_obj_set_style_pad_hor((lv_obj_t *)w, p, 0);
}

void stella_widget_set_pad_ver(stella_widget_t w, int32_t p) {
    lv_obj_set_style_pad_ver((lv_obj_t *)w, p, 0);
}

void stella_widget_set_pad_top(stella_widget_t w, int32_t p) {
    lv_obj_set_style_pad_top((lv_obj_t *)w, p, 0);
}

void stella_widget_set_pad_left(stella_widget_t w, int32_t p) {
    lv_obj_set_style_pad_left((lv_obj_t *)w, p, 0);
}

void stella_widget_set_pad_row(stella_widget_t w, int32_t p) {
    lv_obj_set_style_pad_row((lv_obj_t *)w, p, 0);
}

void stella_widget_set_pad_col(stella_widget_t w, int32_t p) {
    lv_obj_set_style_pad_column((lv_obj_t *)w, p, 0);
}

/* =========================================================================
 * Flex layout
 * ========================================================================= */

/* stella_flex_align_t ordinals match lv_flex_align_t — safe to cast. */

void stella_widget_flex_row(
    stella_widget_t w, stella_flex_align_t main_place, stella_flex_align_t cross_place, stella_flex_align_t track_place
) {
    lv_obj_set_flex_flow((lv_obj_t *)w, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        (lv_obj_t *)w, (lv_flex_align_t)main_place, (lv_flex_align_t)cross_place, (lv_flex_align_t)track_place
    );
}

void stella_widget_flex_col(
    stella_widget_t w, stella_flex_align_t main_place, stella_flex_align_t cross_place, stella_flex_align_t track_place
) {
    lv_obj_set_flex_flow((lv_obj_t *)w, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        (lv_obj_t *)w, (lv_flex_align_t)main_place, (lv_flex_align_t)cross_place, (lv_flex_align_t)track_place
    );
}

/* =========================================================================
 * Text / label styling
 * ========================================================================= */

void stella_text_set_color(stella_widget_t w, stella_color_t color) {
    lv_obj_set_style_text_color((lv_obj_t *)w, _col(color), 0);
}

void stella_text_set_font(stella_widget_t w, const stella_font_t *font) {
    lv_obj_set_style_text_font((lv_obj_t *)w, _font(font), 0);
}

void stella_text_set_align(stella_widget_t w, stella_text_align_t align) {
    /* stella_text_align_t ordinals match lv_text_align_t — safe to cast. */
    lv_obj_set_style_text_align((lv_obj_t *)w, (lv_text_align_t)align, 0);
}

void stella_text_set_pad_top(stella_widget_t w, int32_t p) {
    lv_obj_set_style_pad_top((lv_obj_t *)w, p, 0);
}

void stella_label_update(stella_widget_t lbl, const char *text) {
    lv_label_set_text((lv_obj_t *)lbl, text ? text : "");
}

void stella_label_set_long_dot(stella_widget_t lbl) {
    lv_label_set_long_mode((lv_obj_t *)lbl, LV_LABEL_LONG_DOT);
}

/* =========================================================================
 * State / hover
 * ========================================================================= */

void stella_widget_set_hover_bg(stella_widget_t w, stella_color_t color, stella_opa_t opa) {
    lv_obj_set_style_bg_color((lv_obj_t *)w, _col(color), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa((lv_obj_t *)w, _opa(opa), LV_STATE_FOCUSED);
}

/* =========================================================================
 * Flags / misc
 * ========================================================================= */

void stella_widget_no_scroll(stella_widget_t w) {
    lv_obj_clear_flag((lv_obj_t *)w, LV_OBJ_FLAG_SCROLLABLE);
}

void stella_widget_clean(stella_widget_t w) {
    lv_obj_clean((lv_obj_t *)w);
}

/* =========================================================================
 * Events
 * ========================================================================= */

typedef struct {
    stella_event_cb_t cb;
    void *user_data;
} _stella_click_ctx_t;

static void _stella_click_trampoline(lv_event_t *e) {
    _stella_click_ctx_t *ctx = lv_event_get_user_data(e);
    if (ctx && ctx->cb) {
        ctx->cb((stella_widget_t)lv_event_get_target(e), ctx->user_data);
    }
}

void stella_widget_on_click(stella_widget_t w, stella_event_cb_t cb, void *user_data) {
    lv_obj_t *obj = (lv_obj_t *)w;
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);

    _stella_click_ctx_t *ctx = malloc(sizeof(_stella_click_ctx_t));
    if (!ctx) return;
    ctx->cb = cb;
    ctx->user_data = user_data;
    lv_obj_add_event_cb(obj, _stella_click_trampoline, LV_EVENT_CLICKED, ctx);
}

/* =========================================================================
 * Timers
 * ========================================================================= */

/*
 * stella_timer_t wraps lv_timer_t and keeps the caller's cb + user_data so
 * the LVGL callback can forward them without the caller ever seeing lv_timer_t.
 */
struct stella_timer {
    lv_timer_t *lv_timer;
    stella_timer_cb_t cb;
    void *user_data;
};

static void _timer_trampoline(lv_timer_t *t) {
    stella_timer_t *st = lv_timer_get_user_data(t);
    if (st && st->cb) st->cb(st, st->user_data);
}

stella_timer_t *stella_timer_create(stella_timer_cb_t cb, uint32_t period_ms, void *user_data) {
    stella_timer_t *st = malloc(sizeof(stella_timer_t));
    if (!st) return NULL;
    st->cb = cb;
    st->user_data = user_data;
    st->lv_timer = lv_timer_create(_timer_trampoline, period_ms, st);
    if (!st->lv_timer) {
        free(st);
        return NULL;
    }
    return st;
}

void stella_timer_delete(stella_timer_t *st) {
    if (!st) return;
    lv_timer_delete(st->lv_timer);
    free(st);
}

void stella_timer_fire_now(stella_timer_t *st) {
    if (st) lv_timer_ready(st->lv_timer);
}

/* =========================================================================
 * Bar widget
 * ========================================================================= */

stella_widget_t stella_bar_create(stella_widget_t parent, int32_t w, int32_t h) {
    lv_obj_t *bar = lv_bar_create((lv_obj_t *)parent);
    lv_obj_set_size(bar, w, h);
    return (stella_widget_t)bar;
}

void stella_bar_set_range(stella_widget_t bar, int32_t min, int32_t max) {
    lv_bar_set_range((lv_obj_t *)bar, min, max);
}

void stella_bar_set_value(stella_widget_t bar, int32_t value) {
    lv_bar_set_value((lv_obj_t *)bar, value, LV_ANIM_OFF);
}

void stella_bar_set_track_color(stella_widget_t bar, stella_color_t color) {
    lv_obj_set_style_bg_color((lv_obj_t *)bar, _col(color), LV_PART_MAIN);
}

void stella_bar_set_track_radius(stella_widget_t bar, int32_t r) {
    lv_obj_set_style_radius((lv_obj_t *)bar, r, LV_PART_MAIN);
}

void stella_bar_set_indicator_color(stella_widget_t bar, stella_color_t color) {
    lv_obj_set_style_bg_color((lv_obj_t *)bar, _col(color), LV_PART_INDICATOR);
}

void stella_bar_set_indicator_radius(stella_widget_t bar, int32_t r) {
    lv_obj_set_style_radius((lv_obj_t *)bar, r, LV_PART_INDICATOR);
}

/* =========================================================================
 * VesperaOS LVGL filesystem driver
 *
 * Maps LVGL's lv_fs API onto standard POSIX file calls so that
 * lv_image_set_src() can load files by path at runtime.
 *
 * Drive letter: 'V'  →  "V:/usr/share/icons/foo.png"
 *
 * Stella translates the public-facing plain path "/usr/share/icons/foo.png"
 * into the LVGL drive-prefixed form internally; callers never see 'V:'.
 * ========================================================================= */

#define STELLA_FS_DRIVE 'V'

/* Set to 1 once  ssize_t lseek(HANDLE, ssize_t, int)  exists in VesperaOS */
#ifndef VESPERAOS_HAS_LSEEK
#define VESPERAOS_HAS_LSEEK 0
#endif

static lv_fs_res_t _errno_to_lv(void) {
    switch (errno) {
        case ENOENT:
            return LV_FS_RES_NOT_EX;
        case EACCES:
        case EPERM:
            return LV_FS_RES_DENIED;
        case ENOSPC:
            return LV_FS_RES_FULL;
        case ENOMEM:
            return LV_FS_RES_OUT_OF_MEM;
        case EBUSY:
            return LV_FS_RES_BUSY;
        case EINVAL:
            return LV_FS_RES_INV_PARAM;
        default:
            return LV_FS_RES_UNKNOWN;
    }
}

typedef struct {
    HANDLE handle;
    uint64_t pos; /* updated by read / write / seek */
} fs_file_ctx_t;

static void *_fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode) {
    (void)drv;

    int flags;
    if (mode == LV_FS_MODE_RD)
        flags = O_RDONLY;
    else if (mode == LV_FS_MODE_WR)
        flags = O_WRONLY | O_CREAT;
    else /* LV_FS_MODE_RD | LV_FS_MODE_WR */
        flags = O_RDWR;

    HANDLE h = open(path, flags);
    if ((int64_t)h < 0) {
        printf("stella: [fs] open('%s') failed (errno %d)\n", path, errno);
        return NULL;
    }

    fs_file_ctx_t *ctx = malloc(sizeof *ctx);
    if (!ctx) {
        close(h);
        return NULL;
    }
    ctx->handle = h;
    ctx->pos = 0;
    return ctx;
}

static lv_fs_res_t _fs_close(lv_fs_drv_t *drv, void *file_p) {
    (void)drv;
    fs_file_ctx_t *ctx = (fs_file_ctx_t *)file_p;

    lv_fs_res_t res = (close(ctx->handle) == 0) ? LV_FS_RES_OK : _errno_to_lv();
    free(ctx);
    return res;
}

static lv_fs_res_t _fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br) {
    (void)drv;
    fs_file_ctx_t *ctx = (fs_file_ctx_t *)file_p;

    ssize_t n = read(ctx->handle, buf, (size_t)btr);
    if (n < 0) {
        *br = 0;
        return _errno_to_lv();
    }
    *br = (uint32_t)n;
    ctx->pos += (uint64_t)n;
    return LV_FS_RES_OK;
}

static lv_fs_res_t _fs_write(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw) {
    (void)drv;
    fs_file_ctx_t *ctx = (fs_file_ctx_t *)file_p;

    ssize_t n = write(ctx->handle, buf, (size_t)btw);
    if (n < 0) {
        *bw = 0;
        return _errno_to_lv();
    }
    *bw = (uint32_t)n;
    ctx->pos += (uint64_t)n;
    return LV_FS_RES_OK;
}

static lv_fs_res_t _fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence) {
    (void)drv;
    fs_file_ctx_t *ctx = (fs_file_ctx_t *)file_p;

    int w = (whence == LV_FS_SEEK_SET) ? SEEK_SET : (whence == LV_FS_SEEK_CUR) ? SEEK_CUR : SEEK_END;

    ssize_t new_pos = lseek(ctx->handle, (ssize_t)pos, w);
    if (new_pos < 0) return _errno_to_lv();

    ctx->pos = (uint64_t)new_pos;
    return LV_FS_RES_OK;
}

static lv_fs_res_t _fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p) {
    (void)drv;
    const fs_file_ctx_t *ctx = (const fs_file_ctx_t *)file_p;
    *pos_p = (uint32_t)ctx->pos; /* safe for files < 4 GiB */
    return LV_FS_RES_OK;
}

typedef struct {
    DIR_HANDLE handle;
} fs_dir_ctx_t;

static void *_dir_open(lv_fs_drv_t *drv, const char *path) {
    (void)drv;

    DIR_HANDLE dh = opendir(path);
    if ((int64_t)dh < 0) {
        printf("stella: [fs] opendir('%s') failed (errno %d)\n", path, errno);
        return NULL;
    }

    fs_dir_ctx_t *ctx = malloc(sizeof *ctx);
    if (!ctx) {
        closedir(dh);
        return NULL;
    }
    ctx->handle = dh;
    return ctx;
}

static lv_fs_res_t _dir_read(lv_fs_drv_t *drv, void *rddir_p, char *fn, uint32_t fn_len) {
    (void)drv;
    fs_dir_ctx_t *ctx = (fs_dir_ctx_t *)rddir_p;

    dirent_t entry;
    ssize_t r = readdir(ctx->handle, &entry);

    if (r == 0) {
        /* End of directory – signal with empty string, as LVGL expects. */
        fn[0] = '\0';
        return LV_FS_RES_OK;
    }
    if (r < 0) return _errno_to_lv();

    strncpy(fn, entry.name, (size_t)(fn_len - 1));
    fn[fn_len - 1] = '\0';
    return LV_FS_RES_OK;
}

static lv_fs_res_t _dir_close(lv_fs_drv_t *drv, void *rddir_p) {
    (void)drv;
    fs_dir_ctx_t *ctx = (fs_dir_ctx_t *)rddir_p;

    lv_fs_res_t res = (closedir(ctx->handle) == 0) ? LV_FS_RES_OK : _errno_to_lv();
    free(ctx);
    return res;
}

static lv_fs_drv_t g_fs_drv;

void stella_fs_init(void) {
    lv_fs_drv_init(&g_fs_drv);

    g_fs_drv.letter = STELLA_FS_DRIVE;

    /* File operations */
    g_fs_drv.open_cb = _fs_open;
    g_fs_drv.close_cb = _fs_close;
    g_fs_drv.read_cb = _fs_read;
    g_fs_drv.write_cb = _fs_write;
    g_fs_drv.seek_cb = _fs_seek;
    g_fs_drv.tell_cb = _fs_tell;

    /* Directory operations */
    g_fs_drv.dir_open_cb = _dir_open;
    g_fs_drv.dir_read_cb = _dir_read;
    g_fs_drv.dir_close_cb = _dir_close;

    lv_fs_drv_register(&g_fs_drv);
}

/* =========================================================================
 * Image widget — filesystem path
 * ========================================================================= */

/*
 * LVGL path format: "<drive letter>:<absolute path>"
 * We build "V:/usr/share/icons/foo.png" from "/usr/share/icons/foo.png"
 * and store it in a heap string that outlives the widget (LVGL holds a
 * pointer to the source string until the image is deleted).
 *
 * The string is intentionally leaked — desktop icons live forever.
 * If you ever need cleanup, store the pointer and free on widget deletion.
 */
stella_widget_t stella_image_create_from_path(stella_widget_t parent, const char *path, int32_t w, int32_t h) {
    if (!path) return NULL;

    /* Build "V:<path>\0" — LVGL requires the drive-letter prefix to
     * recognise the source as a filesystem path rather than a raw
     * lv_image_dsc_t pointer. */
    size_t len = strlen(path);
    char *lvpath = malloc(len + 3); /* 'V' + ':' + path + '\0' */
    if (!lvpath) return NULL;
    lvpath[0] = STELLA_FS_DRIVE;
    lvpath[1] = ':';
    memcpy(lvpath + 2, path, len + 1);

    lv_obj_t *img = lv_image_create((lv_obj_t *)parent);
    if (w != STELLA_SIZE_CONTENT || h != STELLA_SIZE_CONTENT) {
        lv_obj_set_size(img, _sz(w), _sz(h));
        lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CONTAIN);
    }
    lv_image_set_src(img, lvpath);

    return (stella_widget_t)img;
}

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

int stella_init(void) {
    int n = vbus_subscribe(VBUS_IFACE_DISPLAY, "");
    if (n < 0) {
        printf("Stella: %d vbus_subscribe failed\n", n);
        abort();
    }
    lv_init();
    stella_fs_init();
    return 0;
}

void stella_tick(uint32_t ms) {
    lv_tick_inc(ms);
    lv_timer_handler();
}

void stella_window_on_close(stella_window_t *win, stella_close_cb_t cb, void *user_data) {
    if (!win) return;
    win->close_cb = cb;
    win->close_user_data = user_data;
}

bool stella_window_should_close(stella_window_t *win) {
    return win && win->should_close;
}