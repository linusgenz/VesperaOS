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

#define MAX_WINDOWS 16
#define MICE_BATCH 32

#define SSD_COLOR_TITLE_FG 0xFFD0D0E8  // title text foreground

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

// Pre-baked ARGB cursor pixels. 0x00000000 = transparent (skipped on blit).
// Built once in init_cursor_pixels(), used as fallback when xcursor load fails.
static uint32_t g_cursor_pixels[16 * 16];

#define XCURSOR_PATH "/usr/share/icons/Bibata-Modern-Ice/cursors/left_ptr"
#define XCURSOR_TARGET_SIZE 24

typedef struct crep_window {
    uint32_t id;
    bool active;

    char owner[32];  // realm name of the client (from vbus sender)
    uint64_t create_serial;

    HANDLE sync_shm;
    HANDLE fb_shm;
    crep_sync_t* sync;
    uint32_t* pixels;
    char sync_shm_name[64];
    char fb_shm_name[64];

    uint32_t x, y, w, h;
    uint32_t content_x, content_y;
    uint32_t content_w, content_h;
    uint32_t flags;
    bool fullscreen;

    char title[64];

    RealmID owner_realm_id;

    bool dirty;

    // Titlebar button bounding boxes (absolute screen coords).
    // Set every frame by ssd_draw_decorations(); read by process_mouse().
    struct {
        uint32_t x, y, w, h;
    } btn_close;
    struct {
        uint32_t x, y, w, h;
    } btn_maximize;
    struct {
        uint32_t x, y, w, h;
    } btn_minimize;

    bool maximized;
    bool minimized;
    uint32_t saved_x, saved_y, saved_w, saved_h;
    uint32_t saved_content_w, saved_content_h;
} crep_window_t;

typedef struct {
    int target_fps;
    uint32_t bg_color;

    int ssd_titlebar_h;
    int ssd_border_w;
    uint32_t ssd_color_titlebar;
    uint32_t ssd_color_titlebar_inactive; /* Tinted down for unfocused windows */
    uint32_t ssd_color_border;
    uint32_t ssd_color_title_fg;

    char cursor_xcursor_path[256];
    uint32_t cursor_xcursor_target_size;

    // [ssd.buttons]
    uint32_t ssd_color_btn_close;     // filled circle color (close)
    uint32_t ssd_color_btn_maximize;  // filled circle color (maximize)
    uint32_t ssd_color_btn_minimize;  // filled circle color (minimize)
    int ssd_btn_size;                 // diameter in pixels
    int ssd_btn_margin;               // gap between buttons
    int ssd_btn_right_pad;            // distance from right edge of titlebar

    // [compositor]
    char compositor_desktop_binary[256];
} display_config_t;

typedef struct compositor_state {
    HANDLE fb;
    HANDLE mouse;
    fb_info_t info;

    display_config_t display_cfg;

    crep_window_t windows[MAX_WINDOWS];
    uint32_t window_count;
    uint32_t next_window_id;

    int32_t mx;
    int32_t my;

    crep_window_t* drag_window;  // NULL = no drag
    int32_t drag_grab_x;         // mx - w->x when clicked
    int32_t drag_grab_y;         // my - w->y when clicked
    uint8_t last_buttons;        // Previous button state for edge detection

    bool needs_present;

    // Button hover state — updated in process_mouse(), consumed by ssd_draw_decorations().
    crep_window_t* hover_btn_window;  // NULL = no button hovered
    int hover_btn_idx;                // 0=close, 1=maximize, 2=minimize, -1=none

    // Action fires only when release lands on the same button as press.
    crep_window_t* pressed_btn_window;  // NULL = no button currently held
    int pressed_btn_idx;                // 0=close, 1=maximize, 2=minimize, -1=none

    struct {
        uint32_t top;
        uint32_t bottom;
        uint32_t left;
        uint32_t right;
    } struts;

    RealmID desktop_realm_id;
    bool desktop_spawned;

    // Software backbuffer: width * height * 4 bytes.
    // All compositing happens here; a single blit pushes it to the screen.
    uint32_t* backbuf;

    /* --- Focus & Z-Order -------------------------------------------------- */

    // Currently keyboard-focused window. NULL means no application has focus
    // (e.g. directly after startup before any window is created, or after all
    // windows are minimized). The desktop window never holds this focus.
    crep_window_t* focused_window;

    // Z-order stack: each entry is an index into windows[].
    // z_order[0]           = bottommost window (desktop, always pinned here).
    // z_order[z_count - 1] = topmost window (receives input first).
    // Invariant: every active window appears exactly once in z_order[0..z_count-1].
    int z_order[MAX_WINDOWS];
    int z_count;
} compositor_state_t;

static RealmID realm_id = 0;
static compositor_state_t g_comp;

static loaded_cursor_t g_xcursor;
static bool g_xcursor_ok = false;

static void init_cursor_pixels(void) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            uint8_t t = G_CURSOR_MAP[row][col];
            uint32_t px = 0;
            switch (t) {
                case 1:
                    px = 0xFFFFFFFF;
                    break;  // white (body)
                case 2:
                    px = 0xFF000000;
                    break;  // black (outline)
                default:
                    px = 0x00000000;
                    break;  // transparent
            }
            g_cursor_pixels[row * 16 + col] = px;
        }
    }

    g_xcursor_ok = xcursor_load_file(
        g_comp.display_cfg.cursor_xcursor_path, g_comp.display_cfg.cursor_xcursor_target_size, &g_xcursor
    );
    if (g_xcursor_ok) {
        printf(
            "Crepusculum: cursor loaded from '%s' (%ux%u hot=%u,%u)\n",
            XCURSOR_PATH,
            g_xcursor.width,
            g_xcursor.height,
            g_xcursor.xhot,
            g_xcursor.yhot
        );
    } else {
        printf("Crepusculum: xcursor load failed, using built-in fallback cursor\n");
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
        if (g_comp.windows[i].active && (g_comp.windows[i].owner_realm_id == owner)) return &g_comp.windows[i];
    }
    return NULL;
}

static crep_window_t* alloc_window_slot(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!g_comp.windows[i].active) return &g_comp.windows[i];
    }
    return NULL;
}

static void make_shm_names(uint32_t id, char* sync_out, char* fb_out) {
    snprintf(sync_out, 64, "/crep_sync_%u", id);
    snprintf(fb_out, 64, "/crep_fb_%u", id);
}

/* =========================================================================
 * Z-Order Management
 *
 * The Z-order array is the single source of truth for compositing order and
 * hit-testing. Rules:
 *
 *  - The desktop window occupies z_order[0] and is never moved from there.
 *  - TOPMOST windows (VBUS_DISP_FLAG_TOPMOST) always sit above regular windows.
 *  - All other windows stack between the desktop and the topmost tier.
 *  - z_raise() places a window at the top of the regular tier, just below any
 *    TOPMOST windows that may already be there.
 * ========================================================================= */

/** Slot index of a window pointer into g_comp.windows[]. */
static inline int w_slot(const crep_window_t* w) {
    return (int)(w - g_comp.windows);
}

/** Append a slot to the top of the Z-order stack (used on window creation). */
static void z_add(int slot) {
    if (g_comp.z_count >= MAX_WINDOWS) return;
    g_comp.z_order[g_comp.z_count++] = slot;
}

/** Remove a slot from the Z-order stack (used on window destruction). */
static void z_remove(int slot) {
    for (int i = 0; i < g_comp.z_count; i++) {
        if (g_comp.z_order[i] != slot) continue;
        memmove(&g_comp.z_order[i], &g_comp.z_order[i + 1], (size_t)(g_comp.z_count - i - 1) * sizeof(int));
        g_comp.z_count--;
        return;
    }
}

/** Raise a window to the top of the regular tier (below TOPMOST windows).
 *  The desktop window is never raised; calling this on it is a no-op. */
static void z_raise(int slot) {
    crep_window_t* w = &g_comp.windows[slot];

    // Desktop is permanently pinned at the bottom.
    if (g_comp.desktop_spawned && w->owner_realm_id == g_comp.desktop_realm_id) return;

    // Find the current position of this slot.
    int pos = -1;
    for (int i = 0; i < g_comp.z_count; i++) {
        if (g_comp.z_order[i] == slot) {
            pos = i;
            break;
        }
    }
    if (pos < 0) return;

    // Find the highest position that is NOT a topmost window, scanning from the top.
    int top = g_comp.z_count - 1;
    while (top > pos) {
        const crep_window_t* tw = &g_comp.windows[g_comp.z_order[top]];
        if (tw->active && (tw->flags & VBUS_DISP_FLAG_TOPMOST)) {
            top--;
        } else {
            break;
        }
    }

    if (pos == top) return;  // Already in the correct position.

    memmove(&g_comp.z_order[pos], &g_comp.z_order[pos + 1], (size_t)(top - pos) * sizeof(int));
    g_comp.z_order[top] = slot;
}

/* =========================================================================
 * SHM Window Buffers
 * ========================================================================= */

static int window_alloc_shm(crep_window_t* w) {
    const uint32_t sync_size = sizeof(crep_sync_t);
    const uint32_t fb_size = w->content_w * w->content_h * CREP_BPP;

    w->sync_shm = shm_open(w->sync_shm_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (w->sync_shm == INVALID_HANDLE) return -1;
    if (ftruncate(w->sync_shm, sync_size) == INVALID_HANDLE) return -1;

    w->sync = (crep_sync_t*)mmap(NULL, sync_size, PROT_READ | PROT_WRITE, MAP_SHARED, w->sync_shm, 0);
    if (w->sync == MAP_FAILED) return -1;
    memset(w->sync, 0, sync_size);

    w->fb_shm = shm_open(w->fb_shm_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (w->fb_shm == INVALID_HANDLE) return -1;
    if (ftruncate(w->fb_shm, fb_size) == INVALID_HANDLE) return -1;

    w->pixels = (uint32_t*)mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, w->fb_shm, 0);
    if (w->pixels == MAP_FAILED) return -1;

    for (uint32_t i = 0; i < w->content_w * w->content_h; i++) w->pixels[i] = g_comp.display_cfg.bg_color;

    // Publish geometry; ready=1 is the release barrier for the client.
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
    if (w->pixels && w->pixels != (void*)MAP_FAILED) munmap(w->pixels, w->content_w * w->content_h * CREP_BPP);
    shm_unlink(w->sync_shm_name);
    shm_unlink(w->fb_shm_name);
    w->sync = NULL;
    w->pixels = NULL;
}

/* =========================================================================
 * VBUS Message Handlers
 * ========================================================================= */

// Forward declarations needed because handle_destroy_window calls window_focus_next,
// which is defined later alongside window_set_focus.
static void window_set_focus(crep_window_t* w);
static void window_focus_next(void);

static void handle_create_window(const vbus_header_t* hdr, const vbus_display_create_window_t* req) {
    printf(
        "Crepusculum: CreateWindow from '%u' (%ux%u flags=0x%x title='%.63s')\n",
        hdr->sender_id,
        req->width,
        req->height,
        req->flags,
        req->title
    );

    vbus_display_window_info_t resp = {0};

    // One window per client realm for now.
    if (find_window_by_owner(hdr->sender_id)) {
        printf("Crepusculum: '%u' already has a window\n", hdr->sender_id);
        resp.status = -EEXIST;
        goto send_return;
    }

    crep_window_t* w = alloc_window_slot();
    if (!w) {
        printf("Crepusculum: window table full\n");
        resp.status = -ENOMEM;
        goto send_return;
    }

    memset(w, 0, sizeof(*w));
    w->id = g_comp.next_window_id++;
    w->flags = req->flags;

    const display_config_t* cfg = &g_comp.display_cfg;
    const int tb = cfg->ssd_titlebar_h;
    const int bw = cfg->ssd_border_w;

    const bool fullscreen = (req->flags & VBUS_DISP_FLAG_FULLSCREEN) || (!req->width || !req->height);
    w->fullscreen = fullscreen;
    if (fullscreen) {
        w->content_w = g_comp.info.width;
        w->content_h = g_comp.info.height;
        w->content_x = 0;
        w->content_y = 0;
        w->w = w->content_w;
        w->h = w->content_h;
        w->x = 0;
        w->y = 0;
    } else {
        w->content_w = req->width;
        w->content_h = req->height;
        w->w = req->width + bw * 2;
        w->h = req->height + tb + bw;
        w->content_x = bw;
        w->content_y = tb;
        w->x = 0;
        w->y = 0;
    }

    strncpy(w->title, req->title, sizeof(w->title) - 1);
    w->title[sizeof(w->title) - 1] = '\0';

    w->owner_realm_id = hdr->sender_id;
    w->create_serial = hdr->serial;
    make_shm_names(w->id, w->sync_shm_name, w->fb_shm_name);

    if (window_alloc_shm(w) < 0) {
        printf("Crepusculum: SHM allocation failed for window %u\n", w->id);
        resp.status = -ENOMEM;
        goto send_return;
    }

    w->active = true;
    g_comp.window_count++;

    /* --- Z-Order Registration -------------------------------------------
     * The desktop window is pinned at the very bottom (z_order[0]).
     * Every other window is pushed to the top of the stack and immediately
     * receives focus so that new windows always open in front. */
    {
        int slot = w_slot(w);
        if (g_comp.desktop_spawned && w->owner_realm_id == g_comp.desktop_realm_id) {
            // Desktop: insert at position 0, shifting everything else up.
            memmove(&g_comp.z_order[1], &g_comp.z_order[0], (size_t)g_comp.z_count * sizeof(int));
            g_comp.z_order[0] = slot;
            g_comp.z_count++;
        } else {
            z_add(slot);  // New application window → top of the stack.
        }
    }

    if (g_comp.desktop_spawned && w->owner_realm_id != g_comp.desktop_realm_id) {
        vbus_display_window_opened_t notif = {.window_id = w->id};
        strncpy(notif.title, w->title, sizeof(notif.title) - 1);
        vbus_signal_to(
            VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_OPENED, realm_id, g_comp.desktop_realm_id, &notif, sizeof(notif)
        );
    }

    if (w->owner_realm_id != g_comp.desktop_realm_id) {
        window_set_focus(w);

        if (g_comp.desktop_spawned) {
            crep_window_t* prev = g_comp.focused_window;
            const uint32_t prev_id = prev ? prev->id : 0;

            const vbus_display_window_focused_t focus_notif = {.window_id = w->id, .prev_window_id = prev_id};
            vbus_signal_to(
                VBUS_IFACE_DISPLAY,
                VBUS_DISP_WINDOW_FOCUSED,
                realm_id,
                g_comp.desktop_realm_id,
                &focus_notif,
                sizeof(focus_notif)
            );
        }
    }

    resp.window_id = w->id;
    resp.status = 0;
    resp.width = w->content_w;
    resp.height = w->content_h;
    strncpy(resp.sync_shm, w->sync_shm_name, sizeof(resp.sync_shm) - 1);
    strncpy(resp.fb_shm, w->fb_shm_name, sizeof(resp.fb_shm) - 1);

    printf("Crepusculum: window %u (%ux%u) created for '%s'\n", w->id, w->w, w->h, w->owner);

send_return:;
    vbus_header_t ret_hdr = {0};
    ret_hdr.type = VBUS_MSG_RETURN;
    ret_hdr.serial = vbus_next_serial();
    ret_hdr.reply_serial = hdr->serial;
    strncpy(ret_hdr.interface, VBUS_IFACE_DISPLAY, sizeof(ret_hdr.interface) - 1);
    strncpy(ret_hdr.member, VBUS_DISP_WINDOW_CREATED, sizeof(ret_hdr.member) - 1);
    vbus_emit_raw(&ret_hdr, &resp, sizeof(resp));
}

static void handle_window_commit(const vbus_header_t* hdr, const vbus_display_commit_t* sig) {
    (void)hdr;
    crep_window_t* w = find_window_by_id(sig->window_id);
    if (!w) return;

    // The vbus signal delivery guarantees the client's pixel writes are visible;
    // just clear the client-side dirty flag (RELAXED is fine here).
    __atomic_store_n(&w->sync->dirty, 0u, __ATOMIC_RELAXED);
    w->dirty = true;
    g_comp.needs_present = true;
}

static void handle_destroy_window(const vbus_header_t* hdr, const vbus_display_window_id_t* req) {
    (void)hdr;
    crep_window_t* w = find_window_by_id(req->window_id);
    if (!w) return;

    printf("Crepusculum: destroying window %u (owner='%s')\n", w->id, w->owner);
    window_free_shm(w);

    // Clear any compositor state that holds a pointer to this window.
    if (g_comp.drag_window == w) g_comp.drag_window = NULL;
    if (g_comp.hover_btn_window == w) {
        g_comp.hover_btn_window = NULL;
        g_comp.hover_btn_idx = -1;
    }
    if (g_comp.pressed_btn_window == w) {
        g_comp.pressed_btn_window = NULL;
        g_comp.pressed_btn_idx = -1;
    }

    // Remember whether this window held focus before we remove it.
    const bool was_focused = (g_comp.focused_window == w);
    if (was_focused) g_comp.focused_window = NULL;

    // Remove from Z-order before the slot is recycled.
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

    // If the destroyed window held focus, pass it to the next eligible window.
    if (was_focused) window_focus_next();
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

static void window_apply_maximize(crep_window_t* w) {
    const display_config_t* cfg = &g_comp.display_cfg;
    uint32_t wa_x, wa_y, wa_w, wa_h;
    crep_get_work_area(&wa_x, &wa_y, &wa_w, &wa_h);

    w->x = wa_x;
    w->y = wa_y;
    w->w = wa_w;
    w->h = wa_h;
    w->content_x = (uint32_t)cfg->ssd_border_w;
    w->content_y = (uint32_t)cfg->ssd_titlebar_h;
    w->content_w = wa_w - (uint32_t)(cfg->ssd_border_w * 2);
    w->content_h = wa_h - (uint32_t)(cfg->ssd_titlebar_h + cfg->ssd_border_w);

    window_resize_shm(w);

    const vbus_display_configure_t conf = {
        .window_id = w->id,
        .width = w->content_w,
        .height = w->content_h,
    };
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
    } else {
        w->maximized = false;
        w->x = w->saved_x;
        w->y = w->saved_y;
        w->w = w->saved_w;
        w->h = w->saved_h;
        w->content_w = w->saved_content_w;
        w->content_h = w->saved_content_h;

        window_resize_shm(w);

        const vbus_display_configure_t conf = {
            .window_id = w->id,
            .width = w->content_w,
            .height = w->content_h,
        };
        vbus_signal_to(
            VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_CONFIGURE, realm_id, w->owner_realm_id, &conf, sizeof(conf)
        );
    }
    g_comp.needs_present = true;
}

// Hide the window from the compositor and notify the desktop shell.
// The window's client is still running; it just isn't composited anymore.
// If the window is already minimized this is a no-op.
static void window_minimize(crep_window_t* w) {
    if (w->minimized) return;

    // Clear any compositor state that references this window.
    if (g_comp.drag_window == w) g_comp.drag_window = NULL;
    if (g_comp.hover_btn_window == w) {
        g_comp.hover_btn_window = NULL;
        g_comp.hover_btn_idx = -1;
    }
    if (g_comp.pressed_btn_window == w) {
        g_comp.pressed_btn_window = NULL;
        g_comp.pressed_btn_idx = -1;
    }

    w->minimized = true;
    g_comp.needs_present = true;

    // If this window held focus, explicitly notify it and pass focus onward.
    if (g_comp.focused_window == w) {
        const vbus_display_window_id_t notif = {.window_id = w->id, ._pad = 0};
        vbus_signal_to(VBUS_IFACE_DISPLAY, VBUS_DISP_FOCUS_LOST, realm_id, w->owner_realm_id, &notif, sizeof(notif));
        g_comp.focused_window = NULL;
        window_focus_next();
    }

    // Tell the desktop shell so it can update the taskbar button.
    if (g_comp.desktop_spawned && w->owner_realm_id != g_comp.desktop_realm_id) {
        const vbus_display_window_id_t notif = {.window_id = w->id, ._pad = 0};
        vbus_signal_to(
            VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_MINIMIZED, realm_id, g_comp.desktop_realm_id, &notif, sizeof(notif)
        );
    }
}

// Make a previously minimized window visible again and notify the desktop shell.
// If the window was maximized before minimizing it comes back maximized.
// If the window is not minimized this is a no-op.
static void window_restore(crep_window_t* w) {
    w->minimized = false;
    g_comp.needs_present = true;

    // Tell the desktop shell so it can update the taskbar button.
    if (g_comp.desktop_spawned && w->owner_realm_id != g_comp.desktop_realm_id) {
        const vbus_display_window_id_t notif = {.window_id = w->id, ._pad = 0};
        vbus_signal_to(
            VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_RESTORED, realm_id, g_comp.desktop_realm_id, &notif, sizeof(notif)
        );
    }
}

/* =========================================================================
 * Focus Management
 * ========================================================================= */

/** Set keyboard focus to w, raise it in Z-order, and broadcast focus signals.
 *
 * Desktop windows never receive application focus. Calling this with the
 * already-focused window still re-raises it (useful after restore).
 */
static void window_set_focus(crep_window_t* w) {
    if (!w || !w->active) return;

    // Desktop window never holds application focus.
    if (g_comp.desktop_spawned && w->owner_realm_id == g_comp.desktop_realm_id) return;

    crep_window_t* prev = g_comp.focused_window;
    const uint32_t prev_id = prev ? prev->id : 0;

    // Always raise, even if already focused.
    z_raise(w_slot(w));

    if (w == prev) {
        // Already focused — just raised and repainted.
        g_comp.needs_present = true;
        return;
    }

    // Notify the old focus holder that it lost focus.
    if (prev) {
        const vbus_display_window_id_t notif = {.window_id = prev->id, ._pad = 0};
        vbus_signal_to(VBUS_IFACE_DISPLAY, VBUS_DISP_FOCUS_LOST, realm_id, prev->owner_realm_id, &notif, sizeof(notif));
    }

    g_comp.focused_window = w;

    // Notify the new focus holder that it gained focus.
    {
        const vbus_display_window_id_t notif = {.window_id = w->id, ._pad = 0};
        vbus_signal_to(VBUS_IFACE_DISPLAY, VBUS_DISP_FOCUS_GAINED, realm_id, w->owner_realm_id, &notif, sizeof(notif));
    }

    // Notify the desktop shell so the taskbar can highlight the correct button.
    if (g_comp.desktop_spawned) {
        const vbus_display_window_focused_t notif = {
            .window_id = w->id,
            .prev_window_id = prev_id,
        };
        vbus_signal_to(
            VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_FOCUSED, realm_id, g_comp.desktop_realm_id, &notif, sizeof(notif)
        );
    }

    g_comp.needs_present = true;
}

/** Transfer focus to the topmost eligible (non-desktop, non-minimized) window.
 *  If no eligible window exists, clears focus and notifies the desktop that
 *  window_id=0 is now active (so the taskbar can deselect all buttons). */
static void window_focus_next(void) {
    if (g_comp.z_count < 0 || g_comp.z_count > MAX_WINDOWS) return;
    for (int zi = g_comp.z_count - 1; zi >= 0; zi--) {
        crep_window_t* w = &g_comp.windows[g_comp.z_order[zi]];
        if (!w->active || !w->pixels || w->minimized) continue;
        if (g_comp.desktop_spawned && w->owner_realm_id == g_comp.desktop_realm_id) continue;
        window_set_focus(w);
        return;
    }

    // No eligible window — tell the desktop that nothing is focused.
    g_comp.focused_window = NULL;
    if (g_comp.desktop_spawned) {
        const vbus_display_window_focused_t notif = {.window_id = 0, .prev_window_id = 0};
        vbus_signal_to(
            VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_FOCUSED, realm_id, g_comp.desktop_realm_id, &notif, sizeof(notif)
        );
    }
}

// Toggle the minimized state of a window, driven by a taskbar click from the desktop.
//
// Behaviour:
//   Minimized                       → restore + focus
//   Visible, not focused            → focus (bring to front), do NOT minimize
//   Visible, focused                → minimize (focus transferred by window_minimize)
static void window_toggle_minimize(crep_window_t* w) {
    if (w->minimized) {
        window_restore(w);
        window_set_focus(w);
    } else if (w != g_comp.focused_window) {
        window_set_focus(w);
    } else {
        window_minimize(w);  // window_minimize handles focus transfer internally
    }
}

/* =========================================================================
 * VBUS Drain
 * ========================================================================= */

static int drain_vbus(void) {
    int processed = 0;
    for (;;) {
        vbus_header_t hdr;
        vbus_payload_t payload;

        int r = vbus_recv(&hdr, &payload, sizeof(payload));
        if (r <= 0) break;

        if (strncmp(hdr.interface, VBUS_IFACE_DISPLAY, 48) != 0) continue;

        if (hdr.type == VBUS_MSG_CALL && strncmp(hdr.member, VBUS_DISP_CREATE_WINDOW, 48) == 0) {
            handle_create_window(&hdr, &payload.create_window);
        } else if (hdr.type == VBUS_MSG_SIGNAL && strncmp(hdr.member, VBUS_DISP_WINDOW_COMMIT, 48) == 0) {
            handle_window_commit(&hdr, &payload.commit);
        } else if (hdr.type == VBUS_MSG_CALL && strncmp(hdr.member, VBUS_DISP_DESTROY_WINDOW, 48) == 0) {
            handle_destroy_window(&hdr, &payload.destroy_window);
        } else if (hdr.type == VBUS_MSG_SIGNAL && strncmp(hdr.member, VBUS_DISP_WINDOW_ACTIVATE, 48) == 0) {
            const vbus_display_window_id_t msg = payload.activate;
            for (int i = 0; i < MAX_WINDOWS; i++) {
                if (g_comp.windows[i].active && g_comp.windows[i].id == msg.window_id) {
                    window_toggle_minimize(&g_comp.windows[i]);
                    break;
                }
            }
        } else if (hdr.type == VBUS_MSG_SIGNAL && strncmp(hdr.member, VBUS_DISP_SET_STRUT, 48) == 0) {
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

/* =========================================================================
 * Backbuffer Compositing
 * ========================================================================= */

// Fill the entire backbuffer with BG_COLOR.
static void backbuf_clear(void) {
    uint32_t total = g_comp.info.width * g_comp.info.height;
    for (uint32_t i = 0; i < total; i++) g_comp.backbuf[i] = g_comp.display_cfg.bg_color;
}

// Copy a window's pixel buffer into the backbuffer, clipped to screen bounds.
// Windows are assumed fully opaque; no alpha blending needed here.
static void backbuf_blit_window(const crep_window_t* w) {
    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;

    const uint32_t dst_x0 = w->x + w->content_x;
    const uint32_t dst_y0 = w->y + w->content_y;
    uint32_t dst_x1 = dst_x0 + w->content_w;
    uint32_t dst_y1 = dst_y0 + w->content_h;

    if (dst_x0 >= sw) return;
    if (dst_y0 >= sh) return;
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

// Alpha-blend a cursor pixel array into the backbuffer.
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

            const uint32_t c = src_pixels[row * w + col];
            const uint8_t a = (uint8_t)(c >> 24);
            if (a == 0) continue;

            if (a == 0xFF) {
                g_comp.backbuf[(uint32_t)py * sw + (uint32_t)px] = c;
            } else {
                const uint32_t bg = g_comp.backbuf[(uint32_t)py * sw + (uint32_t)px];
                const uint32_t inv = 255u - a;
                const uint8_t r = (uint8_t)(((c >> 16 & 0xFF) * a + (bg >> 16 & 0xFF) * inv) / 255);
                const uint8_t g = (uint8_t)(((c >> 8 & 0xFF) * a + (bg >> 8 & 0xFF) * inv) / 255);
                const uint8_t b = (uint8_t)(((c & 0xFF) * a + (bg & 0xFF) * inv) / 255);
                g_comp.backbuf[(uint32_t)py * sw + (uint32_t)px] =
                    0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
        }
    }
}

// Draw the cursor into the backbuffer.
static void backbuf_draw_cursor(void) {
    if (g_xcursor_ok) {
        const int32_t origin_x = g_comp.mx - (int32_t)g_xcursor.xhot;
        const int32_t origin_y = g_comp.my - (int32_t)g_xcursor.yhot;
        backbuf_blit_cursor_pixels(g_xcursor.pixels, g_xcursor.width, g_xcursor.height, origin_x, origin_y);
    } else {
        backbuf_blit_cursor_pixels(g_cursor_pixels, 16, 16, g_comp.mx, g_comp.my);
    }
}

/* =========================================================================
 * Glyph Rendering
 * ========================================================================= */

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
    const uint32_t code = (uint8_t)ch;
    const uint32_t idx = get_glyph_index(code);
    const lv_font_fmt_txt_glyph_dsc_t* dsc = &glyph_dsc[idx];

    if (dsc->box_w == 0 || dsc->box_h == 0) return;

    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;

    const int32_t font_line_height = 15;
    const int32_t font_base_line = 3;

    const int32_t start_y = (int32_t)py + (font_line_height - font_base_line) - dsc->box_h - dsc->ofs_y;
    const int32_t start_x = (int32_t)px + dsc->ofs_x;

    const uint8_t fg_r = (color >> 16) & 0xFF;
    const uint8_t fg_g = (color >> 8) & 0xFF;
    const uint8_t fg_b = color & 0xFF;

    uint32_t bit_ptr = dsc->bitmap_index * 8;

    for (uint16_t row = 0; row < dsc->box_h; row++) {
        const int32_t current_y = start_y + row;

        for (uint16_t col = 0; col < dsc->box_w; col++) {
            const int32_t current_x = start_x + col;
            const uint8_t byte_val = glyph_bitmap[bit_ptr / 8];
            uint8_t alpha4;

            if ((bit_ptr % 8) == 0) {
                alpha4 = (byte_val >> 4) & 0x0F;
            } else {
                alpha4 = byte_val & 0x0F;
            }
            bit_ptr += 4;

            if (alpha4 > 0 && current_y >= 0 && (uint32_t)current_y < sh && current_x >= 0 &&
                (uint32_t)current_x < sw) {
                const uint32_t buf_idx = (uint32_t)current_y * sw + (uint32_t)current_x;

                if (alpha4 == 15) {
                    g_comp.backbuf[buf_idx] = color;
                } else {
                    const uint32_t alpha = (alpha4 * 255) / 15;
                    const uint32_t inv_alpha = 255 - alpha;
                    const uint32_t bg_color = g_comp.backbuf[buf_idx];
                    const uint8_t bg_r = (bg_color >> 16) & 0xFF;
                    const uint8_t bg_g = (bg_color >> 8) & 0xFF;
                    const uint8_t bg_b = bg_color & 0xFF;
                    const uint8_t out_r = (fg_r * alpha + bg_r * inv_alpha) / 255;
                    const uint8_t out_g = (fg_g * alpha + bg_g * inv_alpha) / 255;
                    const uint8_t out_b = (fg_b * alpha + bg_b * inv_alpha) / 255;
                    g_comp.backbuf[buf_idx] = (0xFF000000) | (out_r << 16) | (out_g << 8) | out_b;
                }
            }
        }
    }
}

static uint32_t ssd_text_width(const char* s) {
    if (!s || !*s) return 0;
    uint32_t total_width = 0;
    while (*s) {
        const uint32_t idx = get_glyph_index((uint8_t)*s);
        total_width += (glyph_dsc[idx].adv_w + 8) / 16;
        s++;
    }
    return total_width;
}

/* =========================================================================
 * SSD Drawing Helpers
 * ========================================================================= */

static void ssd_fill_circle_aa(uint32_t cx, uint32_t cy, uint32_t r, uint32_t color) {
    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;

    const uint8_t cr = (color >> 16) & 0xFF;
    const uint8_t cg = (color >> 8) & 0xFF;
    const uint8_t cb = color & 0xFF;
    const uint8_t ca = (color >> 24) & 0xFF;

    const float rf = (float)r;
    const int32_t ri = (int32_t)r + 1;

    for (int32_t dy = -ri; dy <= ri; dy++) {
        int32_t py = (int32_t)cy + dy;
        if (py < 0 || (uint32_t)py >= sh) continue;

        for (int32_t dx = -ri; dx <= ri; dx++) {
            int32_t px = (int32_t)cx + dx;
            if (px < 0 || (uint32_t)px >= sw) continue;

            float dist = sqrtf((float)(dx * dx + dy * dy));
            float cover = rf + 0.5f - dist;
            if (cover <= 0.0f) continue;
            if (cover > 1.0f) cover = 1.0f;

            uint32_t a = (uint32_t)(cover * (float)ca + 0.5f);
            uint32_t inv = 255u - a;

            uint32_t bg = g_comp.backbuf[(uint32_t)py * sw + (uint32_t)px];
            uint8_t out_r = (uint8_t)((cr * a + ((bg >> 16) & 0xFF) * inv + 127u) / 255u);
            uint8_t out_g = (uint8_t)((cg * a + ((bg >> 8) & 0xFF) * inv + 127u) / 255u);
            uint8_t out_b = (uint8_t)((cb * a + (bg & 0xFF) * inv + 127u) / 255u);

            g_comp.backbuf[(uint32_t)py * sw + (uint32_t)px] =
                0xFF000000u | ((uint32_t)out_r << 16) | ((uint32_t)out_g << 8) | out_b;
        }
    }
}

static void ssd_blit_icon_16(uint32_t cx, uint32_t cy, const uint32_t* icon) {
    int32_t origin_x = (int32_t)cx - 8;
    int32_t origin_y = (int32_t)cy - 8;
    backbuf_blit_cursor_pixels(icon, 16, 16, origin_x, origin_y);
}

// Lighten a color by blending it toward white by `amount` (0–255).
static uint32_t color_lighten(uint32_t c, uint8_t amount) {
    uint8_t r = (uint8_t)((c >> 16) & 0xFF);
    uint8_t g = (uint8_t)((c >> 8) & 0xFF);
    uint8_t b = (uint8_t)(c & 0xFF);
    r = (uint8_t)(r + (uint32_t)(255 - r) * amount / 255);
    g = (uint8_t)(g + (uint32_t)(255 - g) * amount / 255);
    b = (uint8_t)(b + (uint32_t)(255 - b) * amount / 255);
    return (c & 0xFF000000u) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Darken a color by scaling its RGB channels toward black by `amount` (0–255).
static uint32_t color_darken(uint32_t c, uint8_t amount) {
    const uint8_t r = (uint8_t)(((c >> 16) & 0xFF) * (255u - amount) / 255u);
    const uint8_t g = (uint8_t)(((c >> 8) & 0xFF) * (255u - amount) / 255u);
    const uint8_t b = (uint8_t)((c & 0xFF) * (255u - amount) / 255u);
    return (c & 0xFF000000u) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Returns which titlebar button (x,y) hits: 0=close, 1=maximize, 2=minimize, -1=none.
static int hit_test_titlebar_buttons(const crep_window_t* w, int32_t x, int32_t y) {
    if (x >= (int32_t)w->btn_close.x && x < (int32_t)(w->btn_close.x + w->btn_close.w) &&
        y >= (int32_t)w->btn_close.y && y < (int32_t)(w->btn_close.y + w->btn_close.h))
        return 0;
    if (x >= (int32_t)w->btn_maximize.x && x < (int32_t)(w->btn_maximize.x + w->btn_maximize.w) &&
        y >= (int32_t)w->btn_maximize.y && y < (int32_t)(w->btn_maximize.y + w->btn_maximize.h))
        return 1;
    if (x >= (int32_t)w->btn_minimize.x && x < (int32_t)(w->btn_minimize.x + w->btn_minimize.w) &&
        y >= (int32_t)w->btn_minimize.y && y < (int32_t)(w->btn_minimize.y + w->btn_minimize.h))
        return 2;
    return -1;
}

static void ssd_hline(uint32_t x0, uint32_t y, uint32_t len, uint32_t color) {
    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;
    if (y >= sh) return;
    uint32_t x1 = x0 + len;
    if (x1 > sw) x1 = sw;
    if (x0 >= x1) return;
    uint32_t* row = &g_comp.backbuf[y * sw + x0];
    for (uint32_t i = 0; i < x1 - x0; i++) row[i] = color;
}

static void ssd_vline(uint32_t x, uint32_t y0, uint32_t len, uint32_t color) {
    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;
    if (x >= sw) return;
    uint32_t y1 = y0 + len;
    if (y1 > sh) y1 = sh;
    for (uint32_t y = y0; y < y1; y++) g_comp.backbuf[y * sw + x] = color;
}

static void ssd_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;
    uint32_t x1 = x + w;
    uint32_t y1 = y + h;
    if (x1 > sw) x1 = sw;
    if (y1 > sh) y1 = sh;
    if (x >= x1 || y >= y1) return;
    for (uint32_t row = y; row < y1; row++) {
        uint32_t* dst = &g_comp.backbuf[row * sw + x];
        for (uint32_t col = 0; col < x1 - x; col++) dst[col] = color;
    }
}

// Render the complete SSD chrome for one windowed (non-fullscreen) window.
// Also writes the btn_close/btn_maximize/btn_minimize bounding boxes so that
// process_mouse() can hit-test without re-computing the layout.
// Called after the window's content has been blitted.
static void ssd_draw_decorations(crep_window_t* w) {
    const display_config_t* cfg = &g_comp.display_cfg;

    const uint32_t ox = w->x;
    const uint32_t oy = w->y;
    const uint32_t ow = w->w;
    const uint32_t oh = w->h;
    const uint32_t tb = (uint32_t)cfg->ssd_titlebar_h;

    // Focused windows get the full titlebar color; inactive ones are dimmed.
    const bool win_focused = (g_comp.focused_window == w);
    const uint32_t tb_color = win_focused ? cfg->ssd_color_titlebar : cfg->ssd_color_titlebar_inactive;
    const uint32_t title_fg = win_focused ? cfg->ssd_color_title_fg : color_darken(cfg->ssd_color_title_fg, 90);

    ssd_fill_rect(ox, oy, ow, tb, tb_color);

    // --- Titlebar buttons (right side: close → max → min) ---
    const uint32_t btn_d = (uint32_t)cfg->ssd_btn_size;
    const uint32_t btn_r = btn_d / 2;
    const uint32_t btn_m = (uint32_t)cfg->ssd_btn_margin;
    const uint32_t btn_rp = (uint32_t)cfg->ssd_btn_right_pad;
    const uint32_t btn_cy = oy + tb / 2;

    const uint32_t close_cx = ox + ow - btn_rp - btn_r;
    const uint32_t max_cx = close_cx - btn_d - btn_m;
    const uint32_t min_cx = max_cx - btn_d - btn_m;

    // Derive button colors; dim them when the window is not focused.
    uint32_t col_close = win_focused ? cfg->ssd_color_btn_close : color_darken(cfg->ssd_color_btn_close, 80);
    uint32_t col_max = win_focused ? cfg->ssd_color_btn_maximize : color_darken(cfg->ssd_color_btn_maximize, 80);
    uint32_t col_min = win_focused ? cfg->ssd_color_btn_minimize : color_darken(cfg->ssd_color_btn_minimize, 80);

    const bool hover_this = (g_comp.hover_btn_window == w);
    const bool pressed_this = (g_comp.pressed_btn_window == w);

    if (pressed_this && g_comp.pressed_btn_idx == 0) col_close = color_lighten(col_close, 160);
    if (pressed_this && g_comp.pressed_btn_idx == 1) col_max = color_lighten(col_max, 160);
    if (pressed_this && g_comp.pressed_btn_idx == 2) col_min = color_lighten(col_min, 160);
    if (!pressed_this && hover_this && g_comp.hover_btn_idx == 0) col_close = color_lighten(col_close, 60);
    if (!pressed_this && hover_this && g_comp.hover_btn_idx == 1) col_max = color_lighten(col_max, 60);
    if (!pressed_this && hover_this && g_comp.hover_btn_idx == 2) col_min = color_lighten(col_min, 60);

    ssd_fill_circle_aa(close_cx, btn_cy, btn_r, col_close);
    ssd_fill_circle_aa(max_cx, btn_cy, btn_r, col_max);
    ssd_fill_circle_aa(min_cx, btn_cy, btn_r, col_min);

    ssd_blit_icon_16(close_cx, btn_cy, window_close_symbolic_16px);
    ssd_blit_icon_16(max_cx, btn_cy, window_maximize_symbolic_16px);
    ssd_blit_icon_16(min_cx, btn_cy, window_minimize_symbolic_16px);

    // Store hit-boxes (square bounding box around each circle).
    w->btn_close.x = close_cx - btn_r;
    w->btn_close.y = btn_cy - btn_r;
    w->btn_close.w = btn_d;
    w->btn_close.h = btn_d;
    w->btn_maximize.x = max_cx - btn_r;
    w->btn_maximize.y = btn_cy - btn_r;
    w->btn_maximize.w = btn_d;
    w->btn_maximize.h = btn_d;
    w->btn_minimize.x = min_cx - btn_r;
    w->btn_minimize.y = btn_cy - btn_r;
    w->btn_minimize.w = btn_d;
    w->btn_minimize.h = btn_d;

    // --- Title text: centered horizontally, avoiding the button area ---
    const uint32_t text_w = ssd_text_width(w->title);
    int32_t title_x = (int32_t)ox + ((int32_t)ow - (int32_t)text_w) / 2;
    const uint32_t title_y = oy + (tb - 15) / 2;
    if (title_x < (int32_t)ox) title_x = (int32_t)ox;

    const char* p = w->title;
    uint32_t gx = (uint32_t)title_x;
    while (*p) {
        const int idx = get_glyph_index((uint8_t)*p);
        ssd_draw_glyph(gx, title_y, *p, title_fg);
        gx += (glyph_dsc[idx].adv_w + 8) / 16;
        p++;
    }

    // --- Borders ---
    ssd_hline(ox, oy + tb - 1, ow, cfg->ssd_color_border);
    ssd_vline(ox, oy, oh, cfg->ssd_color_border);
    if (ow >= 1) ssd_vline(ox + ow - 1, oy, oh, cfg->ssd_color_border);
    if (oh >= 1) ssd_hline(ox, oy + oh - 1, ow, cfg->ssd_color_border);
}

/* =========================================================================
 * Compositor Loop
 * ========================================================================= */

// Composite all visible windows in Z-order (bottom to top), draw the cursor,
// and present the finished backbuffer to the framebuffer.
static void composite_frame(void) {
    backbuf_clear();

    // Walk Z-order from bottom to top so each window is drawn over the previous.
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
        .src_x = 0,
        .src_y = 0,
        .width = g_comp.info.width,
        .height = g_comp.info.height,
        .dst_x = 0,
        .dst_y = 0,
    };
    ioctl(g_comp.fb, FB_IOCTL_BLIT, &full_blit);
    ioctl(g_comp.fb, FB_IOCTL_PRESENT, NULL);

    g_comp.needs_present = false;
}

// Return the topmost window under (x, y), or NULL if only the desktop / nothing is there.
// Iterates Z-order from top to bottom so that the foreground window is found first.
static crep_window_t* find_window_at(int32_t x, int32_t y) {
    for (int zi = g_comp.z_count - 1; zi >= 0; zi--) {
        crep_window_t* w = &g_comp.windows[g_comp.z_order[zi]];
        if (!w->active || !w->pixels || w->minimized) continue;
        if (x >= (int32_t)w->x && x < (int32_t)(w->x + w->w) && y >= (int32_t)w->y && y < (int32_t)(w->y + w->h)) {
            return w;
        }
    }
    return NULL;
}

static bool is_titlebar_hit(const crep_window_t* w, int32_t x, int32_t y) {
    if (w->fullscreen) return false;
    return x >= (int32_t)w->x && x < (int32_t)(w->x + w->w) && y >= (int32_t)w->y && y < (int32_t)(w->y + w->content_y);
}

/* =========================================================================
 * Mouse Input Processing
 * ========================================================================= */

static bool process_mouse(void) {
    mice_event events[MICE_BATCH];
    ssize_t bytes = read(g_comp.mouse, events, sizeof(events));
    if (bytes <= 0 || (size_t)bytes < sizeof(mice_event)) return false;

    size_t count = (size_t)bytes / sizeof(mice_event);
    bool moved = false;

    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;

    crep_window_t* coalesced_win = NULL;
    vbus_display_input_event_t coalesced_payload = {0};

    for (size_t i = 0; i < count; i++) {
        mice_event* ev = &events[i];

        if (ev->type == MICE_EVENT_MOVE) {
            g_comp.mx += ev->dx;
            g_comp.my += ev->dy;
            if (g_comp.mx < 0) g_comp.mx = 0;
            if (g_comp.mx > (int32_t)g_comp.info.width - 1) g_comp.mx = (int32_t)g_comp.info.width - 1;
            if (g_comp.my < 0) g_comp.my = 0;
            if (g_comp.my > (int32_t)g_comp.info.height - 1) g_comp.my = (int32_t)g_comp.info.height - 1;
            moved = true;

            // Update hover state for titlebar button highlighting.
            crep_window_t* prev_hover_win = g_comp.hover_btn_window;
            int prev_hover_idx = g_comp.hover_btn_idx;
            g_comp.hover_btn_window = NULL;
            g_comp.hover_btn_idx = -1;

            for (int wi = g_comp.z_count - 1; wi >= 0; wi--) {
                crep_window_t* w = &g_comp.windows[g_comp.z_order[wi]];
                if (!w->active || !w->pixels || w->fullscreen || w->minimized) continue;
                if (!is_titlebar_hit(w, g_comp.mx, g_comp.my)) continue;
                int btn = hit_test_titlebar_buttons(w, g_comp.mx, g_comp.my);
                if (btn >= 0) {
                    g_comp.hover_btn_window = w;
                    g_comp.hover_btn_idx = btn;
                }
                break;
            }

            if (g_comp.hover_btn_window != prev_hover_win || g_comp.hover_btn_idx != prev_hover_idx)
                g_comp.needs_present = true;

            // Update drag position.
            if (g_comp.drag_window) {
                crep_window_t* w = g_comp.drag_window;

                int32_t new_x = g_comp.mx - g_comp.drag_grab_x;
                int32_t new_y = g_comp.my - g_comp.drag_grab_y;

                if (new_x < 0) new_x = 0;
                if (new_y < 0) new_y = 0;
                if ((uint32_t)new_x + w->w > sw) new_x = (int32_t)(sw - w->w);
                if ((uint32_t)new_y + w->h > sh) new_y = (int32_t)(sh - w->h);

                w->x = (uint32_t)new_x;
                w->y = (uint32_t)new_y;
                g_comp.needs_present = true;

                g_comp.last_buttons = ev->buttons;
                continue;
            }

            // Coalesce: remember the last move event for the window under the cursor.
            crep_window_t* target_win = find_window_at(g_comp.mx, g_comp.my);
            if (target_win) {
                int32_t local_x = g_comp.mx - (int32_t)(target_win->x + target_win->content_x);
                int32_t local_y = g_comp.my - (int32_t)(target_win->y + target_win->content_y);

                if (local_x >= 0 && local_y >= 0 && (uint32_t)local_x < target_win->content_w &&
                    (uint32_t)local_y < target_win->content_h) {
                    coalesced_win = target_win;
                    coalesced_payload = (vbus_display_input_event_t){
                        .window_id = target_win->id,
                        .local_x = local_x,
                        .local_y = local_y,
                        .buttons = ev->buttons,
                        .type = (uint32_t)ev->type,
                    };
                }
            }

            g_comp.last_buttons = ev->buttons;
            continue;
        }

        // --- Button Events (Press / Release) ---

        uint8_t pressed = (uint8_t)(ev->buttons & ~g_comp.last_buttons);
        uint8_t released = (uint8_t)(~ev->buttons & g_comp.last_buttons);
        g_comp.last_buttons = ev->buttons;

        if ((pressed & 1) && !g_comp.drag_window) {
            crep_window_t* w = find_window_at(g_comp.mx, g_comp.my);
            if (w) {
                /* Any left-click on a non-desktop window focuses and raises it.
                 * This happens unconditionally — whether the click lands on the
                 * titlebar, the buttons, or the window content. */
                if (!g_comp.desktop_spawned || w->owner_realm_id != g_comp.desktop_realm_id) {
                    window_set_focus(w);
                }

                if (is_titlebar_hit(w, g_comp.mx, g_comp.my)) {
                    int btn = hit_test_titlebar_buttons(w, g_comp.mx, g_comp.my);
                    if (btn >= 0) {
                        // Remember the button; fire the action only on release.
                        g_comp.pressed_btn_window = w;
                        g_comp.pressed_btn_idx = btn;
                        g_comp.needs_present = true;
                        continue;
                    }
                    // No button hit → start dragging (only if not maximized).
                    if (!w->maximized) {
                        g_comp.drag_window = w;
                        g_comp.drag_grab_x = g_comp.mx - (int32_t)w->x;
                        g_comp.drag_grab_y = g_comp.my - (int32_t)w->y;
                    }
                    continue;
                }
            }
        }

        // Button-Release: fire action only when the cursor is still over the same button.
        if ((released & 1) && g_comp.pressed_btn_window) {
            crep_window_t* w = g_comp.pressed_btn_window;
            int btn = g_comp.pressed_btn_idx;

            g_comp.pressed_btn_window = NULL;
            g_comp.pressed_btn_idx = -1;
            g_comp.needs_present = true;

            crep_window_t* w_under = find_window_at(g_comp.mx, g_comp.my);
            if (w_under == w && is_titlebar_hit(w, g_comp.mx, g_comp.my)) {
                int btn_under = hit_test_titlebar_buttons(w, g_comp.mx, g_comp.my);
                if (btn_under == btn) {
                    if (btn == 0) {
                        vbus_display_window_id_t req = {.window_id = w->id, ._pad = 0};
                        handle_destroy_window(NULL, &req);
                    }
                    if (btn == 1) window_toggle_maximize(w);
                    if (btn == 2) window_minimize(w);
                }
            }
            continue;
        }

        if (g_comp.drag_window) {
            crep_window_t* w = g_comp.drag_window;

            int32_t new_x = g_comp.mx - g_comp.drag_grab_x;
            int32_t new_y = g_comp.my - g_comp.drag_grab_y;

            if (new_x < 0) new_x = 0;
            if (new_y < 0) new_y = 0;
            if ((uint32_t)new_x + w->w > sw) new_x = (int32_t)(sw - w->w);
            if ((uint32_t)new_y + w->h > sh) new_y = (int32_t)(sh - w->h);

            w->x = (uint32_t)new_x;
            w->y = (uint32_t)new_y;
            g_comp.needs_present = true;

            if (released & 1) g_comp.drag_window = NULL;
            continue;
        }

        // Forward button events to the client under the cursor.
        const crep_window_t* target_win = find_window_at(g_comp.mx, g_comp.my);
        if (target_win) {
            const int32_t local_x = g_comp.mx - (int32_t)(target_win->x + target_win->content_x);
            const int32_t local_y = g_comp.my - (int32_t)(target_win->y + target_win->content_y);

            if (local_x < 0 || local_y < 0 || (uint32_t)local_x >= target_win->content_w ||
                (uint32_t)local_y >= target_win->content_h) {
                continue;
            }

            vbus_display_input_event_t input_payload = {
                .window_id = target_win->id,
                .local_x = local_x,
                .local_y = local_y,
                .buttons = ev->buttons,
                .type = (uint32_t)ev->type,
            };

            vbus_signal_to(
                VBUS_IFACE_DISPLAY,
                VBUS_DISP_INPUT_EVENT,
                realm_id,
                target_win->owner_realm_id,
                &input_payload,
                sizeof(input_payload)
            );
        }
    }

    // Send the single coalesced move event for this batch.
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

static inline int64_t now_ns(void) {
    timespec_t ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void sleep_ns(int64_t ns) {
    if (ns <= 0) return;
    const timespec_t ts = {
        .tv_sec = ns / 1000000000LL,
        .tv_nsec = ns % 1000000000LL,
    };
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
        strncpy(
            cfg->cursor_xcursor_path,
            "/usr/share/icons/Bibata-Modern-Ice/cursors/left_ptr",
            sizeof(cfg->cursor_xcursor_path) - 1
        );
        strncpy(cfg->compositor_desktop_binary, "/bin/firmament", sizeof(cfg->compositor_desktop_binary) - 1);
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
        L,
        "cursor",
        "xcursor_path",
        cfg->cursor_xcursor_path,
        sizeof(cfg->cursor_xcursor_path) - 1,
        "/usr/share/icons/Bibata-Modern-Ice/cursors/left_ptr"
    );

    lua_get_table_string(
        L,
        "compositor",
        "desktop_binary",
        cfg->compositor_desktop_binary,
        sizeof(cfg->compositor_desktop_binary) - 1,
        "/bin/firmament"
    );

    printf(
        "Crepusculum: config loaded from '%s' (%d fps, desktop='%s')\n",
        path,
        cfg->target_fps,
        cfg->compositor_desktop_binary
    );
    return true;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    memset(&g_comp, 0, sizeof(g_comp));
    g_comp.next_window_id = 1;
    g_comp.hover_btn_window = NULL;
    g_comp.hover_btn_idx = -1;
    g_comp.pressed_btn_window = NULL;
    g_comp.pressed_btn_idx = -1;
    g_comp.focused_window = NULL;
    g_comp.z_count = 0;

    realm_id = get_realm_id();

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    load_display_config(L, "/etc/crepusculum.lua", &g_comp.display_cfg);
    lua_close(L);

    g_comp.fb = open("/dev/fb0", O_RDONLY);
    if ((int64_t)g_comp.fb <= 0) {
        printf("Crepusculum: cannot open /dev/fb0\n");
        return 1;
    }
    g_comp.mouse = open("/dev/mice", O_RDONLY);
    if ((int64_t)g_comp.mouse < 0) {
        printf("Crepusculum: cannot open /dev/mice\n");
        return 1;
    }
    if (ioctl(g_comp.fb, FB_IOCTL_GET_INFO, &g_comp.info) < 0) {
        printf("Crepusculum: FB_IOCTL_GET_INFO failed\n");
        return 1;
    }

    printf("Crepusculum: framebuffer %ux%u\n", g_comp.info.width, g_comp.info.height);

    uint32_t backbuf_size = (size_t)g_comp.info.width * g_comp.info.height * sizeof(uint32_t);
    g_comp.backbuf = (uint32_t*)mmap(NULL, backbuf_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_comp.backbuf == MAP_FAILED) {
        printf("Crepusculum: backbuffer allocation failed\n");
        return 1;
    }

    init_cursor_pixels();

    fb_clear_t clr = {.color = g_comp.display_cfg.bg_color};
    ioctl(g_comp.fb, FB_IOCTL_CLEAR, &clr);
    ioctl(g_comp.fb, FB_IOCTL_PRESENT, NULL);

    if (vbus_subscribe(VBUS_IFACE_DISPLAY, "") < 0) {
        printf("Crepusculum: vbus_subscribe failed\n");
        return 1;
    }
    printf("Crepusculum: subscribed to %s\n", VBUS_IFACE_DISPLAY);

    g_comp.mx = (int32_t)(g_comp.info.width / 2);
    g_comp.my = (int32_t)(g_comp.info.height / 2);

    const char* desktop_argv[] = {g_comp.display_cfg.compositor_desktop_binary, NULL};
    const char* desktop_envp[] = {"PATH=/bin", "TERM=tty0", NULL};

    HANDLE app_log = open("/var/log/myapp.log", O_WRONLY | O_CREAT | O_TRUNC);

    spawn_config_t cfg = {
        .stdin_handle = 0,
        .stdout_handle = app_log,
        .stderr_handle = app_log,
        .bg_realm = 1,
    };

    int64_t rid =
        spawn_realm(g_comp.display_cfg.compositor_desktop_binary, (char**)desktop_argv, (char**)desktop_envp, &cfg);
    if (rid < 0)
        printf("Crepusculum: desktop spawn failed (%lld), running without desktop\n", (int64_t)rid);
    else {
        printf("Crepusculum: desktop spawned (realm %lld)\n", (RealmID)rid);
        g_comp.desktop_realm_id = (RealmID)rid;
        g_comp.desktop_spawned = true;
    }

    composite_frame();
    printf("Crepusculum: entering compositor loop (%d fps cap)\n", g_comp.display_cfg.target_fps);
    const uint32_t frame_ns = (1000000000LL / g_comp.display_cfg.target_fps);

    int64_t next_frame = now_ns() + frame_ns;

    while (true) {
        const bool mouse_moved = process_mouse();
        drain_vbus();

        if (mouse_moved) g_comp.needs_present = true;

        if (g_comp.needs_present) composite_frame();

        const int64_t remaining = next_frame - now_ns();
        sleep_ns(remaining);
        next_frame += frame_ns;
    }

    return 0;
}