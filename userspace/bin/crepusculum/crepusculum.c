#include <crepusculum_protocol.h>
#include <math.h>
#include <realm.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <time.h>
#include <vbus.h>
#include <vespera/dev/ioctl_framebuffer.h>
#include <vespera/dev/mice.h>
#include <vespera/fflags.h>
#include <vespera/handles.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include "luautil.h"
#include "monteserrat_12.c"
#include "window_close_symbolic_16px.h"
#include "window_maximize_symbolic_16px.h"
#include "window_minimize_symbolic_16px.h"
#include "xcursor_loader.h"

/* --- Constants & Macros --------------------------------------------------- */
#define MAX_WINDOWS 16
#define MICE_BATCH 32

// Limits
#define STR_MAX_PATH 256
#define STR_MAX_SHM 64
#define STR_MAX_TITLE 64
#define STR_MAX_OWNER 32

// Typography
#define FONT_LINE_HEIGHT 15
#define FONT_BASE_LINE 3

// Fallbacks
#define FALLBACK_XCURSOR_PATH "/usr/share/icons/Bibata-Modern-Ice/cursors/left_ptr"
#define FALLBACK_DESKTOP_BIN "/bin/firmament"

// Mouse Buttons
#define BTN_CLOSE_IDX 0
#define BTN_MAXIMIZE_IDX 1
#define BTN_MINIMIZE_IDX 2
#define BTN_NONE_IDX (-1)

/* Resize handle geometry */
#define RESIZE_ZONE    6   /* px vom Rand (innen) = Resize-Trefferzone */
#define RESIZE_CORNER  20  /* px von der Ecke für diagonalen Handle  */
#define MIN_CONTENT_W  160 /* minimale Content-Breite beim Resize     */
#define MIN_CONTENT_H  80  /* minimale Content-Höhe beim Resize       */

typedef enum {
    RESIZE_NONE = 0,
    RESIZE_S, RESIZE_E, RESIZE_W,
    RESIZE_SE, RESIZE_SW,
} resize_edge_t;

static const uint8_t G_CURSOR_MAP[16][16] = {
    {2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
    {2, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 0},
    {2, 1, 1, 1, 2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, 1, 2, 2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 2, 2, 0, 0, 2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0}
};

static uint32_t g_cursor_pixels[16 * 16];

typedef struct rect {
    uint32_t x, y, w, h;
} rect_t;

typedef struct crep_window {
    uint32_t id;
    bool active;
    bool dirty;
    bool fullscreen;
    bool maximized;
    bool minimized;

    char owner[STR_MAX_OWNER];
    char title[STR_MAX_TITLE];
    RealmID owner_realm_id;
    uint64_t create_serial;

    HANDLE sync_shm;
    HANDLE fb_shm;
    char sync_shm_name[STR_MAX_SHM];
    char fb_shm_name[STR_MAX_SHM];

    crep_sync_t* sync;
    uint32_t* pixels;

    uint32_t x, y, w, h;
    uint32_t content_x, content_y;
    uint32_t content_w, content_h;
    uint32_t flags;

    // Decoration bounding boxes
    rect_t btn_close;
    rect_t btn_maximize;
    rect_t btn_minimize;

    // Saved geometry for un-maximizing
    uint32_t saved_x, saved_y, saved_w, saved_h;
    uint32_t saved_content_w, saved_content_h;
} crep_window_t;

typedef struct {
    int target_fps;
    uint32_t bg_color;

    int ssd_titlebar_h;
    int ssd_border_w;
    uint32_t ssd_color_titlebar;
    uint32_t ssd_color_titlebar_inactive;
    uint32_t ssd_color_border;
    uint32_t ssd_color_title_fg;

    char cursor_xcursor_path[STR_MAX_PATH];
    uint32_t cursor_xcursor_target_size;

    uint32_t ssd_color_btn_close;
    uint32_t ssd_color_btn_maximize;
    uint32_t ssd_color_btn_minimize;
    int ssd_btn_size;
    int ssd_btn_margin;
    int ssd_btn_right_pad;

    char compositor_desktop_binary[STR_MAX_PATH];
} display_config_t;

typedef struct compositor_state {
    HANDLE fb;
    HANDLE mouse;
    fb_info_t info;
    display_config_t display_cfg;

    crep_window_t windows[MAX_WINDOWS];
    uint32_t window_count;
    uint32_t next_window_id;

    int32_t mx, my;
    uint8_t last_buttons;
    bool needs_present;

    crep_window_t* drag_window;
    int32_t drag_grab_x, drag_grab_y;

    crep_window_t* resize_window;
    resize_edge_t resize_edge;
    int32_t resize_grab_x, resize_grab_y;
    uint32_t resize_orig_x, resize_orig_y;
    uint32_t resize_orig_w, resize_orig_h;

    crep_window_t* hover_btn_window;
    int hover_btn_idx;
    crep_window_t* pressed_btn_window;
    int pressed_btn_idx;

    struct {
        uint32_t top, bottom, left, right;
    } struts;

    RealmID desktop_realm_id;
    bool desktop_spawned;

    uint32_t* backbuf;

    crep_window_t* focused_window;
    int z_order[MAX_WINDOWS];
    int z_count;
} compositor_state_t;

static RealmID realm_id = 0;
static compositor_state_t g_comp;
static loaded_cursor_t g_xcursor;
static bool g_xcursor_ok = false;

/* --- Color Helpers -------------------------------------------------------- */
static uint8_t color_get_a(uint32_t c) {
    return (c >> 24) & 0xFF;
}

static uint8_t color_get_r(uint32_t c) {
    return (c >> 16) & 0xFF;
}

static uint8_t color_get_g(uint32_t c) {
    return (c >> 8) & 0xFF;
}

static uint8_t color_get_b(uint32_t c) {
    return c & 0xFF;
}

static uint32_t color_make(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static uint32_t color_blend(uint32_t fg, uint32_t bg, uint8_t alpha) {
    if (alpha == 255) return fg;
    if (alpha == 0) return bg;
    uint32_t inv_alpha = 255 - alpha;
    uint8_t r = (color_get_r(fg) * alpha + color_get_r(bg) * inv_alpha) / 255;
    uint8_t g = (color_get_g(fg) * alpha + color_get_g(bg) * inv_alpha) / 255;
    uint8_t b = (color_get_b(fg) * alpha + color_get_b(bg) * inv_alpha) / 255;
    return color_make(255, r, g, b);
}

static uint32_t color_lighten(uint32_t c, uint8_t amount) {
    uint8_t r = color_get_r(c);
    uint8_t g = color_get_g(c);
    uint8_t b = color_get_b(c);
    r += (uint8_t)((255 - r) * amount / 255);
    g += (uint8_t)((255 - g) * amount / 255);
    b += (uint8_t)((255 - b) * amount / 255);
    return color_make(color_get_a(c), r, g, b);
}

static uint32_t color_darken(uint32_t c, uint8_t amount) {
    uint8_t r = (uint8_t)(color_get_r(c) * (255u - amount) / 255u);
    uint8_t g = (uint8_t)(color_get_g(c) * (255u - amount) / 255u);
    uint8_t b = (uint8_t)(color_get_b(c) * (255u - amount) / 255u);
    return color_make(color_get_a(c), r, g, b);
}

/* --- Initialization & Basics ---------------------------------------------- */
static void init_cursor_pixels(void) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            switch (G_CURSOR_MAP[row][col]) {
            case 1:
                g_cursor_pixels[row * 16 + col] = 0xFFFFFFFF;
                break;
            case 2:
                g_cursor_pixels[row * 16 + col] = 0xFF000000;
                break;
            default:
                g_cursor_pixels[row * 16 + col] = 0x00000000;
                break;
            }
        }
    }

    g_xcursor_ok = xcursor_load_file(
        g_comp.display_cfg.cursor_xcursor_path, g_comp.display_cfg.cursor_xcursor_target_size, &g_xcursor
    );

    if (g_xcursor_ok) {
        printf(
            "Crepusculum: cursor loaded from '%s' (%ux%u)\n", FALLBACK_XCURSOR_PATH, g_xcursor.width, g_xcursor.height
        );
    }
    else {
        printf("Crepusculum: xcursor load failed, using built-in fallback\n");
    }
}

static void crep_get_work_area(uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h) {
    *x = g_comp.struts.left;
    *y = g_comp.struts.top;
    *w = g_comp.info.width - g_comp.struts.left - g_comp.struts.right;
    *h = g_comp.info.height - g_comp.struts.top - g_comp.struts.bottom;
}

static crep_window_t* find_window_by_id(uint32_t id) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_comp.windows[i].active && g_comp.windows[i].id == id) return &g_comp.windows[i];
    }
    return NULL;
}

static crep_window_t* find_window_by_owner(uint32_t owner) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_comp.windows[i].active && g_comp.windows[i].owner_realm_id == owner) return &g_comp.windows[i];
    }
    return NULL;
}

static crep_window_t* alloc_window_slot(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!g_comp.windows[i].active) return &g_comp.windows[i];
    }
    return NULL;
}

/* --- Z-Order Management --------------------------------------------------- */
static inline int w_slot(const crep_window_t* w) {
    return (int)(w - g_comp.windows);
}

static void z_add(int slot) {
    if (g_comp.z_count < MAX_WINDOWS) g_comp.z_order[g_comp.z_count++] = slot;
}

static void z_remove(int slot) {
    for (int i = 0; i < g_comp.z_count; i++) {
        if (g_comp.z_order[i] == slot) {
            memmove(&g_comp.z_order[i], &g_comp.z_order[i + 1], (size_t)(g_comp.z_count - i - 1) * sizeof(int));
            g_comp.z_count--;
            return;
        }
    }
}

static void z_raise(int slot) {
    crep_window_t* w = &g_comp.windows[slot];
    if (g_comp.desktop_spawned && w->owner_realm_id == g_comp.desktop_realm_id) return;

    int pos = -1;
    for (int i = 0; i < g_comp.z_count; i++) {
        if (g_comp.z_order[i] == slot) {
            pos = i;
            break;
        }
    }
    if (pos < 0) return;

    int top = g_comp.z_count - 1;
    while (top > pos) {
        if (g_comp.windows[g_comp.z_order[top]].flags & VBUS_DISP_FLAG_TOPMOST)
            top--;
        else
            break;
    }

    if (pos != top) {
        memmove(&g_comp.z_order[pos], &g_comp.z_order[pos + 1], (size_t)(top - pos) * sizeof(int));
        g_comp.z_order[top] = slot;
    }
}

/* --- SHM Buffers ---------------------------------------------------------- */
static int window_alloc_shm(crep_window_t* w) {
    const uint32_t sync_size = sizeof(crep_sync_t);
    const uint32_t fb_size = w->content_w * w->content_h * CREP_BPP;

    w->sync_shm = shm_open(w->sync_shm_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (w->sync_shm == INVALID_HANDLE || ftruncate(w->sync_shm, sync_size) == INVALID_HANDLE) return -1;

    w->sync = (crep_sync_t*)mmap(NULL, sync_size, PROT_READ | PROT_WRITE, MAP_SHARED, w->sync_shm, 0);
    if (w->sync == MAP_FAILED) return -1;
    memset(w->sync, 0, sync_size);

    w->fb_shm = shm_open(w->fb_shm_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (w->fb_shm == INVALID_HANDLE || ftruncate(w->fb_shm, fb_size) == INVALID_HANDLE) return -1;

    w->pixels = (uint32_t*)mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, w->fb_shm, 0);
    if (w->pixels == MAP_FAILED) return -1;

    for (uint32_t i = 0; i < w->content_w * w->content_h; i++) w->pixels[i] = g_comp.display_cfg.bg_color;

    w->sync->width = w->content_w;
    w->sync->height = w->content_h;
    w->sync->bpp = CREP_BPP;
    w->sync->pitch = w->content_w * CREP_BPP;
    w->sync->dirty = 0;
    w->sync->seq = 0;
    __atomic_store_n(&w->sync->magic, (uint32_t)CREP_MAGIC, __ATOMIC_RELEASE);
    __atomic_store_n(&w->sync->ready, 1u, __ATOMIC_RELEASE);

    return 0;
}

static void window_free_shm(crep_window_t* w) {
    if (w->sync && w->sync != MAP_FAILED) munmap(w->sync, sizeof(crep_sync_t));
    if (w->pixels && w->pixels != (void*)MAP_FAILED) munmap(w->pixels, (size_t)w->content_w * w->content_h * CREP_BPP);
    shm_unlink(w->sync_shm_name);
    shm_unlink(w->fb_shm_name);
    w->sync = NULL;
    w->pixels = NULL;
}

static int window_resize_shm(crep_window_t* w) {
    if (w->pixels && w->pixels != MAP_FAILED) {
        munmap(w->pixels, (size_t)w->content_w * w->content_h * CREP_BPP);
        w->pixels = NULL;
    }

    const uint32_t new_fb_size = w->content_w * w->content_h * CREP_BPP;
    if (ftruncate(w->fb_shm, new_fb_size) < 0) return -1;

    w->pixels = (uint32_t*)mmap(NULL, new_fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, w->fb_shm, 0);
    if (w->pixels == MAP_FAILED) return -1;

    w->sync->width = w->content_w;
    w->sync->height = w->content_h;
    w->sync->pitch = w->content_w * CREP_BPP;
    return 0;
}

/* --- Window Actions ------------------------------------------------------- */
static void window_set_focus(crep_window_t* w);
static void window_focus_next(void);

static void window_apply_maximize(crep_window_t* w) {
    uint32_t wa_x, wa_y, wa_w, wa_h;
    crep_get_work_area(&wa_x, &wa_y, &wa_w, &wa_h);

    w->x = wa_x;
    w->y = wa_y;
    w->w = wa_w;
    w->h = wa_h;
    w->content_x = (uint32_t)g_comp.display_cfg.ssd_border_w;
    w->content_y = (uint32_t)g_comp.display_cfg.ssd_titlebar_h;
    w->content_w = wa_w - (uint32_t)(g_comp.display_cfg.ssd_border_w * 2);
    w->content_h = wa_h - (uint32_t)(g_comp.display_cfg.ssd_titlebar_h + g_comp.display_cfg.ssd_border_w);

    window_resize_shm(w);

    const vbus_display_configure_t conf = {.window_id = w->id, .width = w->content_w, .height = w->content_h};
    vbus_signal_to(VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_CONFIGURE, realm_id, w->owner_realm_id, &conf, sizeof(conf));
}

static void window_toggle_maximize(crep_window_t* w) {
    if (g_comp.drag_window == w) g_comp.drag_window = NULL;

    if (!w->maximized) {
        w->saved_x = w->x;
        w->saved_y = w->y;
        w->saved_w = w->w;
        w->saved_h = w->h;
        w->saved_content_w = w->content_w;
        w->saved_content_h = w->content_h;
        w->maximized = true;
        window_apply_maximize(w);
    }
    else {
        w->maximized = false;
        w->x = w->saved_x;
        w->y = w->saved_y;
        w->w = w->saved_w;
        w->h = w->saved_h;
        w->content_w = w->saved_content_w;
        w->content_h = w->saved_content_h;

        window_resize_shm(w);
        const vbus_display_configure_t conf = {.window_id = w->id, .width = w->content_w, .height = w->content_h};
        vbus_signal_to(
            VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_CONFIGURE, realm_id, w->owner_realm_id, &conf, sizeof(conf)
        );
    }
    g_comp.needs_present = true;
}

static void window_minimize(crep_window_t* w) {
    if (w->minimized) return;

    if (g_comp.resize_window == w) g_comp.resize_window = NULL;
    if (g_comp.drag_window == w) g_comp.drag_window = NULL;
    if (g_comp.hover_btn_window == w) {
        g_comp.hover_btn_window = NULL;
        g_comp.hover_btn_idx = BTN_NONE_IDX;
    }
    if (g_comp.pressed_btn_window == w) {
        g_comp.pressed_btn_window = NULL;
        g_comp.pressed_btn_idx = BTN_NONE_IDX;
    }

    w->minimized = true;
    g_comp.needs_present = true;

    if (g_comp.focused_window == w) {
        const vbus_display_window_id_t notif = {.window_id = w->id, ._pad = 0};
        vbus_signal_to(VBUS_IFACE_DISPLAY, VBUS_DISP_FOCUS_LOST, realm_id, w->owner_realm_id, &notif, sizeof(notif));
        g_comp.focused_window = NULL;
        window_focus_next();
    }

    if (g_comp.desktop_spawned && w->owner_realm_id != g_comp.desktop_realm_id) {
        const vbus_display_window_id_t notif = {.window_id = w->id, ._pad = 0};
        vbus_signal_to(
            VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_MINIMIZED, realm_id, g_comp.desktop_realm_id, &notif, sizeof(notif)
        );
    }
}

static void window_restore(crep_window_t* w) {
    w->minimized = false;
    g_comp.needs_present = true;

    if (g_comp.desktop_spawned && w->owner_realm_id != g_comp.desktop_realm_id) {
        const vbus_display_window_id_t notif = {.window_id = w->id, ._pad = 0};
        vbus_signal_to(
            VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_RESTORED, realm_id, g_comp.desktop_realm_id, &notif, sizeof(notif)
        );
    }
}

static void window_toggle_minimize(crep_window_t* w) {
    if (w->minimized) {
        window_restore(w);
        window_set_focus(w);
    }
    else if (w != g_comp.focused_window) {
        window_set_focus(w);
    }
    else {
        window_minimize(w);
    }
}

/* --- Focus Management ----------------------------------------------------- */
static void window_set_focus(crep_window_t* w) {
    if (!w || !w->active) return;
    if (g_comp.desktop_spawned && w->owner_realm_id == g_comp.desktop_realm_id) return;

    crep_window_t* prev = g_comp.focused_window;
    z_raise(w_slot(w));

    if (w == prev) {
        g_comp.needs_present = true;
        return;
    }

    if (prev) {
        const vbus_display_window_id_t notif = {.window_id = prev->id, ._pad = 0};
        vbus_signal_to(VBUS_IFACE_DISPLAY, VBUS_DISP_FOCUS_LOST, realm_id, prev->owner_realm_id, &notif, sizeof(notif));
    }

    g_comp.focused_window = w;

    const vbus_display_window_id_t gained = {.window_id = w->id, ._pad = 0};
    vbus_signal_to(VBUS_IFACE_DISPLAY, VBUS_DISP_FOCUS_GAINED, realm_id, w->owner_realm_id, &gained, sizeof(gained));

    if (g_comp.desktop_spawned) {
        const vbus_display_window_focused_t focus_notif = {.window_id = w->id, .prev_window_id = prev ? prev->id : 0};
        vbus_signal_to(
            VBUS_IFACE_DISPLAY,
            VBUS_DISP_WINDOW_FOCUSED,
            realm_id,
            g_comp.desktop_realm_id,
            &focus_notif,
            sizeof(focus_notif)
        );
    }
    g_comp.needs_present = true;
}

static void window_focus_next(void) {
    if (g_comp.z_count < 0 || g_comp.z_count > MAX_WINDOWS) return;
    for (int zi = g_comp.z_count - 1; zi >= 0; zi--) {
        crep_window_t* w = &g_comp.windows[g_comp.z_order[zi]];
        if (!w->active || !w->pixels || w->minimized) continue;
        if (g_comp.desktop_spawned && w->owner_realm_id == g_comp.desktop_realm_id) continue;
        window_set_focus(w);
        return;
    }

    g_comp.focused_window = NULL;
    if (g_comp.desktop_spawned) {
        const vbus_display_window_focused_t notif = {.window_id = 0, .prev_window_id = 0};
        vbus_signal_to(
            VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_FOCUSED, realm_id, g_comp.desktop_realm_id, &notif, sizeof(notif)
        );
    }
}

/* --- VBUS Message Handlers ------------------------------------------------ */
static void handle_create_window(const vbus_header_t* hdr, const vbus_display_create_window_t* req) {
    vbus_display_window_info_t resp = {0};

    if (find_window_by_owner(hdr->sender_id)) {
        resp.status = -EEXIST;
        goto send_return;
    }

    crep_window_t* w = alloc_window_slot();
    if (!w) {
        resp.status = -ENOMEM;
        goto send_return;
    }

    memset(w, 0, sizeof(*w));
    w->id = g_comp.next_window_id++;
    w->flags = req->flags;
    w->owner_realm_id = hdr->sender_id;
    w->create_serial = hdr->serial;

    const display_config_t* cfg = &g_comp.display_cfg;
    w->fullscreen = (req->flags & VBUS_DISP_FLAG_FULLSCREEN) || (!req->width || !req->height);

    if (w->fullscreen) {
        w->content_w = g_comp.info.width;
        w->content_h = g_comp.info.height;
        w->w = w->content_w;
        w->h = w->content_h;
        w->x = 0;
        w->y = 0;
        w->content_x = 0;
        w->content_y = 0;
    }
    else {
        w->content_w = req->width;
        w->content_h = req->height;
        w->w = req->width + cfg->ssd_border_w * 2;
        w->h = req->height + cfg->ssd_titlebar_h + cfg->ssd_border_w;
        w->content_x = cfg->ssd_border_w;
        w->content_y = cfg->ssd_titlebar_h;
        w->x = 0;
        w->y = 0;
    }

    strlcpy(w->title, req->title, STR_MAX_TITLE);
    snprintf(w->sync_shm_name, STR_MAX_SHM, "/crep_sync_%u", w->id);
    snprintf(w->fb_shm_name, STR_MAX_SHM, "/crep_fb_%u", w->id);

    if (window_alloc_shm(w) < 0) {
        resp.status = -ENOMEM;
        goto send_return;
    }

    w->active = true;
    g_comp.window_count++;

    if (g_comp.desktop_spawned && w->owner_realm_id == g_comp.desktop_realm_id) {
        memmove(&g_comp.z_order[1], &g_comp.z_order[0], (size_t)g_comp.z_count * sizeof(int));
        g_comp.z_order[0] = w_slot(w);
        g_comp.z_count++;
    }
    else {
        z_add(w_slot(w));
    }

    if (g_comp.desktop_spawned && w->owner_realm_id != g_comp.desktop_realm_id) {
        vbus_display_window_opened_t notif = {.window_id = w->id};
        strlcpy(notif.title, w->title, sizeof(notif.title));
        vbus_signal_to(
            VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_OPENED, realm_id, g_comp.desktop_realm_id, &notif, sizeof(notif)
        );
    }

    if (w->owner_realm_id != g_comp.desktop_realm_id) {
        window_set_focus(w);
    }

    resp.window_id = w->id;
    resp.status = 0;
    resp.width = w->content_w;
    resp.height = w->content_h;
    strlcpy(resp.sync_shm, w->sync_shm_name, STR_MAX_SHM);
    strlcpy(resp.fb_shm, w->fb_shm_name, STR_MAX_SHM);

send_return:;
    vbus_header_t ret_hdr = {0};
    ret_hdr.type = VBUS_MSG_RETURN;
    ret_hdr.serial = vbus_next_serial();
    ret_hdr.reply_serial = hdr->serial;
    strcpy(ret_hdr.interface, VBUS_IFACE_DISPLAY);
    strcpy(ret_hdr.member, VBUS_DISP_WINDOW_CREATED);
    vbus_emit_raw(&ret_hdr, &resp, sizeof(resp));
}

static void handle_destroy_window(const vbus_header_t* hdr, const vbus_display_window_id_t* req) {
    (void)hdr;
    crep_window_t* w = find_window_by_id(req->window_id);
    if (!w) return;

    window_free_shm(w);

    if (g_comp.resize_window == w) g_comp.resize_window = NULL;
    if (g_comp.drag_window == w) g_comp.drag_window = NULL;
    if (g_comp.hover_btn_window == w) {
        g_comp.hover_btn_window = NULL;
        g_comp.hover_btn_idx = BTN_NONE_IDX;
    }
    if (g_comp.pressed_btn_window == w) {
        g_comp.pressed_btn_window = NULL;
        g_comp.pressed_btn_idx = BTN_NONE_IDX;
    }

    const bool was_focused = (g_comp.focused_window == w);
    if (was_focused) g_comp.focused_window = NULL;

    z_remove(w_slot(w));
    w->active = false;
    g_comp.window_count--;
    g_comp.needs_present = true;

    const vbus_display_window_id_t closed = {.window_id = req->window_id, ._pad = 0};
    vbus_signal_to(VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_CLOSED, realm_id, w->owner_realm_id, &closed, sizeof(closed));

    if (g_comp.desktop_spawned && w->owner_realm_id != g_comp.desktop_realm_id) {
        vbus_signal_to(
            VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_CLOSED, realm_id, g_comp.desktop_realm_id, &closed, sizeof(closed)
        );
    }

    if (was_focused) window_focus_next();
}

static int drain_vbus(void) {
    int processed = 0;
    for (;;) {
        vbus_header_t hdr;
        vbus_payload_t payload;

        if (vbus_recv(&hdr, &payload, sizeof(payload)) <= 0) break;
        if (strcmp(hdr.interface, VBUS_IFACE_DISPLAY) != 0) continue;

        if (hdr.type == VBUS_MSG_CALL && strcmp(hdr.member, VBUS_DISP_CREATE_WINDOW) == 0) {
            handle_create_window(&hdr, &payload.create_window);
        }
        else if (hdr.type == VBUS_MSG_SIGNAL && strcmp(hdr.member, VBUS_DISP_WINDOW_COMMIT) == 0) {
            crep_window_t* w = find_window_by_id(payload.commit.window_id);
            if (w) {
                __atomic_store_n(&w->sync->dirty, 0u, __ATOMIC_RELAXED);
                w->dirty = true;
                g_comp.needs_present = true;
            }
        }
        else if (hdr.type == VBUS_MSG_CALL && strcmp(hdr.member, VBUS_DISP_DESTROY_WINDOW) == 0) {
            handle_destroy_window(&hdr, &payload.destroy_window);
        }
        else if (hdr.type == VBUS_MSG_SIGNAL && strcmp(hdr.member, VBUS_DISP_WINDOW_ACTIVATE) == 0) {
            crep_window_t* w = find_window_by_id(payload.activate.window_id);
            if (w) window_toggle_minimize(w);
        }
        else if (hdr.type == VBUS_MSG_SIGNAL && strcmp(hdr.member, VBUS_DISP_SET_STRUT) == 0) {
            const vbus_display_set_strut_t* s = &payload.set_strut;
            switch (s->edge) {
            case CREP_STRUT_TOP:
                g_comp.struts.top = s->size;
                break;
            case CREP_STRUT_BOTTOM:
                g_comp.struts.bottom = s->size;
                break;
            case CREP_STRUT_LEFT:
                g_comp.struts.left = s->size;
                break;
            case CREP_STRUT_RIGHT:
                g_comp.struts.right = s->size;
                break;
            }
            for (int i = 0; i < MAX_WINDOWS; i++) {
                if (g_comp.windows[i].active && g_comp.windows[i].maximized) window_apply_maximize(&g_comp.windows[i]);
            }
            g_comp.needs_present = true;
        }
        processed++;
    }
    return processed;
}

/* --- Graphics Primitives & Compositing ------------------------------------ */
static void backbuf_clear(void) {
    uint32_t total = g_comp.info.width * g_comp.info.height;
    for (uint32_t i = 0; i < total; i++) g_comp.backbuf[i] = g_comp.display_cfg.bg_color;
}

static void backbuf_blit_window(const crep_window_t* w) {
    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;

    uint32_t dst_x0 = w->x + w->content_x;
    uint32_t dst_y0 = w->y + w->content_y;
    uint32_t dst_x1 = dst_x0 + w->content_w;
    uint32_t dst_y1 = dst_y0 + w->content_h;

    if (dst_x0 >= sw || dst_y0 >= sh) return;
    if (dst_x1 > sw) dst_x1 = sw;
    if (dst_y1 > sh) dst_y1 = sh;
    if (dst_x0 >= dst_x1 || dst_y0 >= dst_y1) return;

    for (uint32_t row = dst_y0; row < dst_y1; row++) {
        uint32_t src_row = row - dst_y0;
        const uint32_t* src = &w->pixels[src_row * w->content_w + (dst_x0 - (w->x + w->content_x))];
        uint32_t* dst = &g_comp.backbuf[row * sw + dst_x0];
        memcpy(dst, src, (dst_x1 - dst_x0) * sizeof(uint32_t));
    }
}

static void backbuf_blit_cursor_pixels(
    const uint32_t* src_pixels, uint32_t w, uint32_t h, int32_t origin_x, int32_t origin_y
) {
    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;

    for (uint32_t row = 0; row < h; row++) {
        const int32_t py = origin_y + (int32_t)row;
        if (py < 0 || (uint32_t)py >= sh) continue;

        for (uint32_t col = 0; col < w; col++) {
            const int32_t px = origin_x + (int32_t)col;
            if (px < 0 || (uint32_t)px >= sw) continue;

            const uint32_t fg = src_pixels[row * w + col];
            const uint8_t a = color_get_a(fg);
            if (a == 0) continue;

            const uint32_t idx = (uint32_t)py * sw + (uint32_t)px;
            g_comp.backbuf[idx] = color_blend(fg, g_comp.backbuf[idx], a);
        }
    }
}

static void backbuf_draw_cursor(void) {
    if (g_xcursor_ok) {
        backbuf_blit_cursor_pixels(
            g_xcursor.pixels, g_xcursor.width, g_xcursor.height, g_comp.mx - g_xcursor.xhot, g_comp.my - g_xcursor.yhot
        );
    }
    else {
        backbuf_blit_cursor_pixels(g_cursor_pixels, 16, 16, g_comp.mx, g_comp.my);
    }
}

static uint32_t get_glyph_index(uint32_t unicode) {
    for (int i = 0; i < 4; i++) {
        const lv_font_fmt_txt_cmap_t* cmap = &cmaps[i];
        if (unicode >= cmap->range_start && unicode < (cmap->range_start + cmap->range_length)) {
            return unicode - cmap->range_start + cmap->glyph_id_start;
        }
    }
    return 0;
}

static void ssd_draw_glyph(const uint32_t px, const uint32_t py, const char ch, const uint32_t color) {
    const uint32_t idx = get_glyph_index((uint8_t)ch);
    const lv_font_fmt_txt_glyph_dsc_t* dsc = &glyph_dsc[idx];
    if (dsc->box_w == 0 || dsc->box_h == 0) return;

    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;

    const int32_t start_y = (int32_t)py + (FONT_LINE_HEIGHT - FONT_BASE_LINE) - dsc->box_h - dsc->ofs_y;
    const int32_t start_x = (int32_t)px + dsc->ofs_x;
    uint32_t bit_ptr = dsc->bitmap_index * 8;

    for (uint16_t row = 0; row < dsc->box_h; row++) {
        const int32_t cy = start_y + row;
        for (uint16_t col = 0; col < dsc->box_w; col++) {
            const int32_t cx = start_x + col;
            const uint8_t byte_val = glyph_bitmap[bit_ptr / 8];
            const uint8_t alpha4 = ((bit_ptr % 8) == 0) ? ((byte_val >> 4) & 0x0F) : (byte_val & 0x0F);
            bit_ptr += 4;

            if (alpha4 > 0 && cy >= 0 && (uint32_t)cy < sh && cx >= 0 && (uint32_t)cx < sw) {
                const uint32_t buf_idx = (uint32_t)cy * sw + (uint32_t)cx;
                const uint8_t alpha8 = (alpha4 * 255) / 15;
                g_comp.backbuf[buf_idx] = color_blend(color, g_comp.backbuf[buf_idx], alpha8);
            }
        }
    }
}

static uint32_t ssd_text_width(const char* s) {
    uint32_t total_width = 0;
    for (; s && *s; s++) {
        total_width += (glyph_dsc[get_glyph_index((uint8_t)*s)].adv_w + 8) / 16;
    }
    return total_width;
}

static void ssd_fill_circle_aa(uint32_t cx, uint32_t cy, uint32_t r, uint32_t color) {
    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;
    const float rf = (float)r;
    const int32_t ri = (int32_t)r + 1;

    for (int32_t dy = -ri; dy <= ri; dy++) {
        int32_t py = (int32_t)cy + dy;
        if (py < 0 || (uint32_t)py >= sh) continue;

        for (int32_t dx = -ri; dx <= ri; dx++) {
            const int32_t px = (int32_t)cx + dx;
            if (px < 0 || (uint32_t)px >= sw) continue;

            float dist = sqrtf((float)(dx * dx + dy * dy));
            float cover = rf + 0.5f - dist;
            if (cover <= 0.0f) continue;
            if (cover > 1.0f) cover = 1.0f;

            const uint8_t a = (uint8_t)(cover * (float)color_get_a(color) + 0.5f);
            const uint32_t idx = (uint32_t)py * sw + (uint32_t)px;
            g_comp.backbuf[idx] = color_blend(color, g_comp.backbuf[idx], a);
        }
    }
}

static void ssd_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;
    const uint32_t x1 = x + w > sw ? sw : x + w;
    const uint32_t y1 = y + h > sh ? sh : y + h;

    if (x >= x1 || y >= y1) return;
    for (uint32_t row = y; row < y1; row++) {
        uint32_t* dst = &g_comp.backbuf[row * sw + x];
        for (uint32_t col = 0; col < x1 - x; col++) dst[col] = color;
    }
}

/* --- Server-Side Decorations (SSD) ---------------------------------------- */
static int hit_test_titlebar_buttons(const crep_window_t* w, int32_t x, int32_t y) {
    if (x >= (int32_t)w->btn_close.x && x < (int32_t)(w->btn_close.x + w->btn_close.w) &&
        y >= (int32_t)w->btn_close.y && y < (int32_t)(w->btn_close.y + w->btn_close.h))
        return BTN_CLOSE_IDX;
    if (x >= (int32_t)w->btn_maximize.x && x < (int32_t)(w->btn_maximize.x + w->btn_maximize.w) &&
        y >= (int32_t)w->btn_maximize.y && y < (int32_t)(w->btn_maximize.y + w->btn_maximize.h))
        return BTN_MAXIMIZE_IDX;
    if (x >= (int32_t)w->btn_minimize.x && x < (int32_t)(w->btn_minimize.x + w->btn_minimize.w) &&
        y >= (int32_t)w->btn_minimize.y && y < (int32_t)(w->btn_minimize.y + w->btn_minimize.h))
        return BTN_MINIMIZE_IDX;
    return BTN_NONE_IDX;
}

static void ssd_draw_decorations(crep_window_t* w) {
    const display_config_t* cfg = &g_comp.display_cfg;
    const bool is_focused = (g_comp.focused_window == w);

    // Background
    const uint32_t tb_color = is_focused ? cfg->ssd_color_titlebar : cfg->ssd_color_titlebar_inactive;
    ssd_fill_rect(w->x, w->y, w->w, cfg->ssd_titlebar_h, tb_color);

    // Title Text
    const uint32_t title_fg = is_focused ? cfg->ssd_color_title_fg : color_darken(cfg->ssd_color_title_fg, 90);
    const uint32_t text_w = ssd_text_width(w->title);
    int32_t title_x = (int32_t)w->x + ((int32_t)w->w - (int32_t)text_w) / 2;
    if (title_x < (int32_t)w->x) title_x = (int32_t)w->x;

    uint32_t gx = (uint32_t)title_x;
    const uint32_t title_y = w->y + (cfg->ssd_titlebar_h - FONT_LINE_HEIGHT) / 2;
    for (const char* p = w->title; *p; p++) {
        ssd_draw_glyph(gx, title_y, *p, title_fg);
        gx += (glyph_dsc[get_glyph_index((uint8_t)*p)].adv_w + 8) / 16;
    }

    // Buttons
    const uint32_t btn_r = cfg->ssd_btn_size / 2;
    const uint32_t btn_cy = w->y + cfg->ssd_titlebar_h / 2;
    const uint32_t cx_close = w->x + w->w - cfg->ssd_btn_right_pad - btn_r;
    const uint32_t cx_max = cx_close - cfg->ssd_btn_size - cfg->ssd_btn_margin;
    const uint32_t cx_min = cx_max - cfg->ssd_btn_size - cfg->ssd_btn_margin;

    uint32_t col_close = is_focused ? cfg->ssd_color_btn_close : color_darken(cfg->ssd_color_btn_close, 80);
    uint32_t col_max = is_focused ? cfg->ssd_color_btn_maximize : color_darken(cfg->ssd_color_btn_maximize, 80);
    uint32_t col_min = is_focused ? cfg->ssd_color_btn_minimize : color_darken(cfg->ssd_color_btn_minimize, 80);

    const bool hover = (g_comp.hover_btn_window == w);
    const bool pressed = (g_comp.pressed_btn_window == w);

    if (pressed && g_comp.pressed_btn_idx == 0)
        col_close = color_lighten(col_close, 160);
    else if (hover && g_comp.hover_btn_idx == 0)
        col_close = color_lighten(col_close, 60);

    if (pressed && g_comp.pressed_btn_idx == 1)
        col_max = color_lighten(col_max, 160);
    else if (hover && g_comp.hover_btn_idx == 1)
        col_max = color_lighten(col_max, 60);

    if (pressed && g_comp.pressed_btn_idx == 2)
        col_min = color_lighten(col_min, 160);
    else if (hover && g_comp.hover_btn_idx == 2)
        col_min = color_lighten(col_min, 60);

    ssd_fill_circle_aa(cx_close, btn_cy, btn_r, col_close);
    ssd_fill_circle_aa(cx_max, btn_cy, btn_r, col_max);
    ssd_fill_circle_aa(cx_min, btn_cy, btn_r, col_min);

    backbuf_blit_cursor_pixels(window_close_symbolic_16px, 16, 16, cx_close - 8, btn_cy - 8);
    backbuf_blit_cursor_pixels(window_maximize_symbolic_16px, 16, 16, cx_max - 8, btn_cy - 8);
    backbuf_blit_cursor_pixels(window_minimize_symbolic_16px, 16, 16, cx_min - 8, btn_cy - 8);

    w->btn_close = (rect_t){cx_close - btn_r, btn_cy - btn_r, cfg->ssd_btn_size, cfg->ssd_btn_size};
    w->btn_maximize = (rect_t){cx_max - btn_r, btn_cy - btn_r, cfg->ssd_btn_size, cfg->ssd_btn_size};
    w->btn_minimize = (rect_t){cx_min - btn_r, btn_cy - btn_r, cfg->ssd_btn_size, cfg->ssd_btn_size};

    // Borders
    ssd_fill_rect(w->x, w->y + cfg->ssd_titlebar_h - 1, w->w, 1, cfg->ssd_color_border); // Bottom title
    ssd_fill_rect(w->x, w->y, 1, w->h, cfg->ssd_color_border); // Left
    if (w->w > 0) ssd_fill_rect(w->x + w->w - 1, w->y, 1, w->h, cfg->ssd_color_border); // Right
    if (w->h > 0) ssd_fill_rect(w->x, w->y + w->h - 1, w->w, 1, cfg->ssd_color_border); // Bottom
}

static void composite_frame(void) {
    backbuf_clear();

    for (int zi = 0; zi < g_comp.z_count; zi++) {
        crep_window_t* w = &g_comp.windows[g_comp.z_order[zi]];
        if (!w->active || !w->pixels || w->minimized) continue;
        backbuf_blit_window(w);
        if (!w->fullscreen) ssd_draw_decorations(w);
        w->dirty = false;
    }

    backbuf_draw_cursor();

    fb_blit_t full_blit = {
        .pixels = g_comp.backbuf,
        .src_stride = g_comp.info.width,
        .src_height = g_comp.info.height,
        .width = g_comp.info.width,
        .height = g_comp.info.height
    };
    ioctl(g_comp.fb, FB_IOCTL_BLIT, &full_blit);
    ioctl(g_comp.fb, FB_IOCTL_PRESENT, NULL);
    g_comp.needs_present = false;
}

/* --- Input Handling ------------------------------------------------------- */
static crep_window_t* find_window_at(int32_t x, int32_t y) {
    for (int zi = g_comp.z_count - 1; zi >= 0; zi--) {
        crep_window_t* w = &g_comp.windows[g_comp.z_order[zi]];
        if (!w->active || !w->pixels || w->minimized) continue;
        if (x >= (int32_t)w->x && x < (int32_t)(w->x + w->w) && y >= (int32_t)w->y && y < (int32_t)(w->y + w->h))
            return w;
    }
    return NULL;
}

static bool is_titlebar_hit(const crep_window_t* w, int32_t x, int32_t y) {
    return !w->fullscreen && x >= (int32_t)w->x && x < (int32_t)(w->x + w->w) && y >= (int32_t)w->y &&
        y < (int32_t)(w->y + w->content_y);
}

static resize_edge_t hit_test_resize_edge(const crep_window_t* w, int32_t mx, int32_t my) {
    if (w->fullscreen || w->maximized) return RESIZE_NONE;

    const int32_t wx = (int32_t)w->x;
    const int32_t wy = (int32_t)w->y;
    const int32_t ww = (int32_t)w->w;
    const int32_t wh = (int32_t)w->h;

    if (mx < wx || mx >= wx + ww || my < wy || my >= wy + wh) return RESIZE_NONE;

    const bool on_left = (mx - wx) < RESIZE_ZONE;
    const bool on_right = (wx + ww - mx) <= RESIZE_ZONE;
    const bool on_bottom = (wy + wh - my) <= RESIZE_ZONE;

    if (!on_left && !on_right && !on_bottom) return RESIZE_NONE;

    const bool near_left = (mx - wx) < RESIZE_CORNER;
    const bool near_right = (wx + ww - mx) <= RESIZE_CORNER;

    if (on_bottom && near_left) return RESIZE_SW;
    if (on_bottom && near_right) return RESIZE_SE;
    if (on_bottom) return RESIZE_S;
    if (on_left) return RESIZE_W;
    if (on_right) return RESIZE_E;

    return RESIZE_NONE;
}

static void window_apply_resize(crep_window_t* w,
                                int32_t new_x, int32_t new_y,
                                int32_t new_w, int32_t new_h) {
    const display_config_t* cfg = &g_comp.display_cfg;
    const int32_t frame_h = cfg->ssd_titlebar_h + cfg->ssd_border_w;
    const int32_t frame_lr = cfg->ssd_border_w * 2;

    const uint32_t new_cw = (uint32_t)(new_w - frame_lr);
    const uint32_t new_ch = (uint32_t)(new_h - frame_h);

    const bool size_changed = (new_cw != w->content_w || new_ch != w->content_h);

    w->x = (uint32_t)new_x;
    w->y = (uint32_t)new_y;
    w->w = (uint32_t)new_w;
    w->h = (uint32_t)new_h;

    if (size_changed) {
        w->content_w = new_cw;
        w->content_h = new_ch;
        window_resize_shm(w);

        const vbus_display_configure_t conf = {
            .window_id = w->id,
            .width = w->content_w,
            .height = w->content_h,
        };
        vbus_signal_to(
            VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_CONFIGURE,
            realm_id, w->owner_realm_id, &conf, sizeof(conf)
        );
    }

    g_comp.needs_present = true;
}

static void handle_resize_move(int32_t mx, int32_t my) {
    crep_window_t* w = g_comp.resize_window;
    const display_config_t* cfg = &g_comp.display_cfg;

    const int32_t dx = mx - g_comp.resize_grab_x;
    const int32_t dy = my - g_comp.resize_grab_y;

    const int32_t frame_h = cfg->ssd_titlebar_h + cfg->ssd_border_w;
    const int32_t frame_lr = cfg->ssd_border_w * 2;
    const int32_t min_w = (int32_t)MIN_CONTENT_W + frame_lr;
    const int32_t min_h = (int32_t)MIN_CONTENT_H + frame_h;

    int32_t new_x = (int32_t)g_comp.resize_orig_x;
    int32_t new_y = (int32_t)g_comp.resize_orig_y;
    int32_t new_w = (int32_t)g_comp.resize_orig_w;
    int32_t new_h = (int32_t)g_comp.resize_orig_h;

    const resize_edge_t e = g_comp.resize_edge;

    if (e == RESIZE_E || e == RESIZE_SE) {
        new_w += dx;
        if (new_w < min_w) new_w = min_w;
        if (new_x + new_w > (int32_t)g_comp.info.width)
            new_w = (int32_t)g_comp.info.width - new_x;
    }
    else if (e == RESIZE_W || e == RESIZE_SW) {
        const int32_t fixed_right = (int32_t)g_comp.resize_orig_x + (int32_t)g_comp.resize_orig_w;
        new_x += dx;
        if (new_x < 0) new_x = 0;
        new_w = fixed_right - new_x;
        if (new_w < min_w) {
            new_x = fixed_right - min_w;
            new_w = min_w;
        }
    }

    if (e == RESIZE_S || e == RESIZE_SE || e == RESIZE_SW) {
        new_h += dy;
        if (new_h < min_h) new_h = min_h;
        if (new_y + new_h > (int32_t)g_comp.info.height)
            new_h = (int32_t)g_comp.info.height - new_y;
    }

    window_apply_resize(w, new_x, new_y, new_w, new_h);
}

static void handle_mouse_move(
    const mice_event* ev, crep_window_t** out_coalesced_win, vbus_display_input_event_t* out_payload
) {
    g_comp.mx += ev->dx;
    g_comp.my += ev->dy;
    if (g_comp.mx < 0) g_comp.mx = 0;
    if (g_comp.mx >= (int32_t)g_comp.info.width) g_comp.mx = (int32_t)g_comp.info.width - 1;
    if (g_comp.my < 0) g_comp.my = 0;
    if (g_comp.my >= (int32_t)g_comp.info.height) g_comp.my = (int32_t)g_comp.info.height - 1;

    crep_window_t* prev_hover_win = g_comp.hover_btn_window;
    int prev_hover_idx = g_comp.hover_btn_idx;
    g_comp.hover_btn_window = NULL;
    g_comp.hover_btn_idx = BTN_NONE_IDX;

    for (int wi = g_comp.z_count - 1; wi >= 0; wi--) {
        crep_window_t* w = &g_comp.windows[g_comp.z_order[wi]];
        if (!w->active || !w->pixels || w->fullscreen || w->minimized) continue;
        if (!is_titlebar_hit(w, g_comp.mx, g_comp.my)) continue;

        int btn = hit_test_titlebar_buttons(w, g_comp.mx, g_comp.my);
        if (btn >= 0) {
            g_comp.hover_btn_window = w;
            g_comp.hover_btn_idx = btn;
            break;
        }
    }

    if (g_comp.hover_btn_window != prev_hover_win || g_comp.hover_btn_idx != prev_hover_idx) {
        g_comp.needs_present = true;
    }

    if (g_comp.drag_window) {
        crep_window_t* w = g_comp.drag_window;
        const int32_t nx = g_comp.mx - g_comp.drag_grab_x;
        const int32_t ny = g_comp.my - g_comp.drag_grab_y;

        w->x = nx < 0 ? 0 : ((uint32_t)nx + w->w > g_comp.info.width ? g_comp.info.width - w->w : (uint32_t)nx);
        w->y = ny < 0 ? 0 : ((uint32_t)ny + w->h > g_comp.info.height ? g_comp.info.height - w->h : (uint32_t)ny);
        g_comp.needs_present = true;
        return;
    }

    if (g_comp.resize_window) {
        handle_resize_move(g_comp.mx, g_comp.my);
        return;
    }

    crep_window_t* target = find_window_at(g_comp.mx, g_comp.my);
    if (target) {
        const int32_t lx = g_comp.mx - (int32_t)(target->x + target->content_x);
        const int32_t ly = g_comp.my - (int32_t)(target->y + target->content_y);
        if (lx >= 0 && ly >= 0 && (uint32_t)lx < target->content_w && (uint32_t)ly < target->content_h) {
            *out_coalesced_win = target;
            *out_payload = (vbus_display_input_event_t){target->id, lx, ly, ev->buttons, ev->type};
        }
    }
}

static bool process_mouse(void) {
    mice_event events[MICE_BATCH];
    const ssize_t bytes = read(g_comp.mouse, events, sizeof(events));
    if (bytes <= 0 || (size_t)bytes < sizeof(mice_event)) return false;

    const size_t count = (size_t)bytes / sizeof(mice_event);
    bool moved = false;
    crep_window_t* coalesced_win = NULL;
    vbus_display_input_event_t coalesced_payload = {0};

    for (size_t i = 0; i < count; i++) {
        const mice_event* ev = &events[i];

        if (ev->type == MICE_EVENT_MOVE) {
            handle_mouse_move(ev, &coalesced_win, &coalesced_payload);
            moved = true;
            g_comp.last_buttons = ev->buttons;
            continue;
        }

        const uint8_t pressed = (uint8_t)(ev->buttons & ~g_comp.last_buttons);
        const uint8_t released = (uint8_t)(~ev->buttons & g_comp.last_buttons);
        g_comp.last_buttons = ev->buttons;

        if ((pressed & 1) && !g_comp.drag_window && !g_comp.resize_window) {
            crep_window_t* w = find_window_at(g_comp.mx, g_comp.my);
            if (w) {
                if (!g_comp.desktop_spawned || w->owner_realm_id != g_comp.desktop_realm_id) window_set_focus(w);

                resize_edge_t edge = hit_test_resize_edge(w, g_comp.mx, g_comp.my);
                if (edge != RESIZE_NONE) {
                    g_comp.resize_window = w;
                    g_comp.resize_edge = edge;
                    g_comp.resize_grab_x = g_comp.mx;
                    g_comp.resize_grab_y = g_comp.my;
                    g_comp.resize_orig_x = w->x;
                    g_comp.resize_orig_y = w->y;
                    g_comp.resize_orig_w = w->w;
                    g_comp.resize_orig_h = w->h;
                    g_comp.needs_present = true;
                    continue;
                }

                if (is_titlebar_hit(w, g_comp.mx, g_comp.my)) {
                    int btn = hit_test_titlebar_buttons(w, g_comp.mx, g_comp.my);
                    if (btn >= 0) {
                        g_comp.pressed_btn_window = w;
                        g_comp.pressed_btn_idx = btn;
                    }
                    else if (!w->maximized) {
                        g_comp.drag_window = w;
                        g_comp.drag_grab_x = g_comp.mx - (int32_t)w->x;
                        g_comp.drag_grab_y = g_comp.my - (int32_t)w->y;
                    }
                    g_comp.needs_present = true;
                    continue;
                }
            }
        }

        if ((released & 1) && g_comp.pressed_btn_window) {
            crep_window_t* w = g_comp.pressed_btn_window;
            const int btn = g_comp.pressed_btn_idx;
            g_comp.pressed_btn_window = NULL;
            g_comp.pressed_btn_idx = BTN_NONE_IDX;
            g_comp.needs_present = true;

            const crep_window_t* w_under = find_window_at(g_comp.mx, g_comp.my);
            if (w_under == w && is_titlebar_hit(w, g_comp.mx, g_comp.my) &&
                hit_test_titlebar_buttons(w, g_comp.mx, g_comp.my) == btn) {
                if (btn == BTN_CLOSE_IDX) {
                    vbus_display_window_id_t req = {w->id, 0};
                    handle_destroy_window(NULL, &req);
                }
                else if (btn == BTN_MAXIMIZE_IDX)
                    window_toggle_maximize(w);
                else if (btn == BTN_MINIMIZE_IDX)
                    window_minimize(w);
            }
            continue;
        }

        if (g_comp.resize_window && (released & 1)) {
            g_comp.resize_window = NULL;
            continue;
        }

        if (g_comp.drag_window && (released & 1)) {
            g_comp.drag_window = NULL;
            continue;
        }

        const crep_window_t* target = find_window_at(g_comp.mx, g_comp.my);
        if (target) {
            const int32_t lx = g_comp.mx - (int32_t)(target->x + target->content_x);
            const int32_t ly = g_comp.my - (int32_t)(target->y + target->content_y);
            if (lx >= 0 && ly >= 0 && (uint32_t)lx < target->content_w && (uint32_t)ly < target->content_h) {
                vbus_display_input_event_t payload = {
                    .window_id = target->id,
                    .local_x = lx,
                    .local_y = ly,
                    .buttons = ev->buttons,
                    .type = (uint32_t)ev->type,
                };

                vbus_signal_to(
                    VBUS_IFACE_DISPLAY,
                    VBUS_DISP_INPUT_EVENT,
                    realm_id,
                    target->owner_realm_id,
                    &payload,
                    sizeof(payload)
                );
            }
        }
    }

    if (coalesced_win) {
        vbus_signal_to(
            VBUS_IFACE_DISPLAY,
            VBUS_DISP_INPUT_EVENT,
            realm_id,
            coalesced_win->owner_realm_id,
            &coalesced_payload,
            sizeof(coalesced_payload)
        );
    }

    return moved;
}

/* --- Timing & Setup ------------------------------------------------------- */
static inline int64_t now_ns(void) {
    timespec_t ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void sleep_ns(const int64_t ns) {
    if (ns <= 0) return;
    const timespec_t ts = {.tv_sec = ns / 1000000000LL, .tv_nsec = ns % 1000000000LL};
    nanosleep(&ts, NULL);
}

bool load_display_config(lua_State* L, const char* path, display_config_t* cfg) {
    if (luaL_dofile(L, path) != LUA_OK) {
        printf("Crepusculum: config error in '%s': %s\n", path, lua_tostring(L, -1));
        lua_pop(L, 1);

        cfg->target_fps = 120;
        cfg->bg_color = 0xFF1A1A24;
        cfg->ssd_titlebar_h = 34;
        cfg->ssd_border_w = 1;
        cfg->ssd_color_titlebar = 0xFF252535;
        cfg->ssd_color_titlebar_inactive = 0xFF1C1C29;
        cfg->ssd_color_border = 0xFF3A3A52;
        cfg->ssd_color_title_fg = 0xFFD0D0E8;
        cfg->ssd_color_btn_close = 0xFFED8796;
        cfg->ssd_color_btn_maximize = 0xFF8EC994;
        cfg->ssd_color_btn_minimize = 0xFFF8D080;
        cfg->ssd_btn_size = 16;
        cfg->ssd_btn_margin = 4;
        cfg->ssd_btn_right_pad = 8;
        cfg->cursor_xcursor_target_size = 24;
        strlcpy(cfg->cursor_xcursor_path, FALLBACK_XCURSOR_PATH, STR_MAX_PATH);
        strlcpy(cfg->compositor_desktop_binary, FALLBACK_DESKTOP_BIN, STR_MAX_PATH);
        return false;
    }

    cfg->target_fps = lua_get_table_int(L, "display", "target_fps", 120);
    cfg->bg_color = (uint32_t)lua_get_table_int(L, "display", "bg_color", 0xFF1A1A24);
    cfg->ssd_titlebar_h = lua_get_table_int(L, "ssd", "titlebar_h", 34);
    cfg->ssd_border_w = lua_get_table_int(L, "ssd", "border_w", 1);
    cfg->ssd_color_titlebar = (uint32_t)lua_get_table_int(L, "ssd", "color_titlebar", 0xFF252535);
    cfg->ssd_color_titlebar_inactive = (uint32_t)lua_get_table_int(L, "ssd", "color_titlebar_inactive", 0xFF1C1C29);
    cfg->ssd_color_border = (uint32_t)lua_get_table_int(L, "ssd", "color_border", 0xFF3A3A52);
    cfg->ssd_color_title_fg = (uint32_t)lua_get_table_int(L, "ssd", "color_title_fg", 0xFFD0D0E8);
    cfg->ssd_color_btn_close = (uint32_t)lua_get_table_int(L, "ssd", "color_btn_close", 0xFFED8796);
    cfg->ssd_color_btn_maximize = (uint32_t)lua_get_table_int(L, "ssd", "color_btn_maximize", 0xFF8EC994);
    cfg->ssd_color_btn_minimize = (uint32_t)lua_get_table_int(L, "ssd", "color_btn_minimize", 0xFFF8D080);
    cfg->ssd_btn_size = (int)lua_get_table_int(L, "ssd", "btn_size", 16);
    cfg->ssd_btn_margin = (int)lua_get_table_int(L, "ssd", "btn_margin", 4);
    cfg->ssd_btn_right_pad = (int)lua_get_table_int(L, "ssd", "btn_right_pad", 8);

    int64_t target_cursor_size = lua_get_table_int(L, "cursor", "xcursor_target_size", 24);
    cfg->cursor_xcursor_target_size = (target_cursor_size > 0) ? (uint32_t)target_cursor_size : 24;

    lua_get_table_string(
        L, "cursor", "xcursor_path", cfg->cursor_xcursor_path, STR_MAX_PATH, FALLBACK_XCURSOR_PATH
    );
    lua_get_table_string(
        L, "compositor", "desktop_binary", cfg->compositor_desktop_binary, STR_MAX_PATH, FALLBACK_DESKTOP_BIN
    );

    return true;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    memset(&g_comp, 0, sizeof(g_comp));
    g_comp.next_window_id = 1;
    g_comp.hover_btn_idx = BTN_NONE_IDX;
    g_comp.pressed_btn_idx = BTN_NONE_IDX;

    realm_id = get_realm_id();

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    load_display_config(L, "/etc/crepusculum.lua", &g_comp.display_cfg);
    lua_close(L);

    g_comp.fb = open("/dev/fb0", O_RDONLY);
    g_comp.mouse = open("/dev/mice", O_RDONLY);
    if ((int64_t)g_comp.fb <= 0 || (int64_t)g_comp.mouse < 0) return 1;

    if (ioctl(g_comp.fb, FB_IOCTL_GET_INFO, &g_comp.info) < 0) return 1;

    const uint32_t backbuf_size = g_comp.info.width * g_comp.info.height * (uint32_t)sizeof(uint32_t);
    g_comp.backbuf = (uint32_t*)mmap(NULL, backbuf_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_comp.backbuf == MAP_FAILED) return 1;

    init_cursor_pixels();

    fb_clear_t clr = {.color = g_comp.display_cfg.bg_color};
    ioctl(g_comp.fb, FB_IOCTL_CLEAR, &clr);
    ioctl(g_comp.fb, FB_IOCTL_PRESENT, NULL);

    if (vbus_subscribe(VBUS_IFACE_DISPLAY, "") < 0) return 1;

    g_comp.mx = g_comp.info.width / 2;
    g_comp.my = g_comp.info.height / 2;

    const char* desktop_argv[] = {g_comp.display_cfg.compositor_desktop_binary, NULL};
    const char* desktop_envp[] = {"PATH=/bin", "TERM=tty0", NULL};
    const HANDLE app_log = open("/var/log/myapp.log", O_WRONLY | O_CREAT | O_TRUNC);

    spawn_config_t cfg = {.stdin_handle = 0, .stdout_handle = app_log, .stderr_handle = app_log, .bg_realm = 1};
    const int64_t rid =
        spawn_realm(g_comp.display_cfg.compositor_desktop_binary, (char**)desktop_argv, (char**)desktop_envp, &cfg);

    if (rid > 0) {
        g_comp.desktop_realm_id = (RealmID)rid;
        g_comp.desktop_spawned = true;
    } else {
        printf("Spawning desktop failed");
    }

    composite_frame();
    const uint32_t frame_ns = (1000000000LL / g_comp.display_cfg.target_fps);
    int64_t next_frame = now_ns() + frame_ns;

    while (true) {
        if (process_mouse()) g_comp.needs_present = true;
        drain_vbus();
        if (g_comp.needs_present) composite_frame();

        sleep_ns(next_frame - now_ns());
        next_frame += frame_ns;
    }

    return 0;
}
