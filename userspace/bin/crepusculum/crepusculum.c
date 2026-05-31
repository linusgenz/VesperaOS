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
#include <vbus_display.h>
#include <vespera/dev/ioctl_framebuffer.h>
#include <vespera/dev/mice.h>
#include <vespera/fflags.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include "luautil.h"
#include "monteserrat_12.c"
#include "vespera/handles.h"
#include "window_close_symbolic_16px.h"
#include "window_maximize_symbolic_16px.h"
#include "window_minimize_symbolic_16px.h"
#include "xcursor_loader.h"

#define MAX_WINDOWS 16
#define MICE_BATCH 32

#define FRAME_NS (1000000000LL / TARGET_FPS)

#define SSD_COLOR_TITLE_FG 0xFFD0D0E8  // title text foreground

static const uint8_t g_cursor_map[16][16] = {
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
} crep_window_t;

typedef struct {
    int target_fps;
    uint32_t bg_color;

    int ssd_titlebar_h;
    int ssd_border_w;
    uint32_t ssd_color_titlebar;
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

    // Software backbuffer: width * height * 4 bytes.
    // All compositing happens here; a single blit pushes it to the screen.
    uint32_t* backbuf;
} compositor_state_t;

static compositor_state_t g_comp;

static loaded_cursor_t g_xcursor;
static bool g_xcursor_ok = false;

static void init_cursor_pixels(void) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            uint8_t t = g_cursor_map[row][col];
            uint32_t px;
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
    if (w->pixels && w->pixels != (void*)MAP_FAILED) munmap(w->pixels, (w->content_w * w->content_h * CREP_BPP));
    shm_unlink(w->sync_shm_name);
    shm_unlink(w->fb_shm_name);
    w->sync = NULL;
    w->pixels = NULL;
}

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

    bool fullscreen = (req->flags & VBUS_DISP_FLAG_FULLSCREEN) || (!req->width || !req->height);
    w->fullscreen = fullscreen;
    if (fullscreen) {
        // Fullscreen: kein SSD, content == outer
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
        // Outer frame = content + SSD-Ränder
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

static void handle_destroy_window(const vbus_header_t* hdr, const vbus_display_destroy_window_t* req) {
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

    w->active = false;
    g_comp.window_count--;
    g_comp.needs_present = true;

    vbus_display_window_id_t closed = {.window_id = req->window_id, ._pad = 0};
    vbus_signal(VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_CLOSED, w->owner_realm_id, &closed, sizeof(closed));
}

static int drain_vbus(void) {
    int processed = 0;
    for (;;) {
        vbus_header_t hdr;
        union {
            vbus_display_create_window_t create;
            vbus_display_commit_t commit;
            vbus_display_destroy_window_t destroy;
        } payload;

        int r = vbus_recv(&hdr, &payload, sizeof(payload));
        if (r <= 0) break;

        if (strncmp(hdr.interface, VBUS_IFACE_DISPLAY, 48) != 0) continue;

        if (hdr.type == VBUS_MSG_CALL && strncmp(hdr.member, VBUS_DISP_CREATE_WINDOW, 48) == 0) {
            handle_create_window(&hdr, &payload.create);
        } else if (hdr.type == VBUS_MSG_SIGNAL && strncmp(hdr.member, VBUS_DISP_WINDOW_COMMIT, 48) == 0) {
            handle_window_commit(&hdr, &payload.commit);
        } else if (hdr.type == VBUS_MSG_CALL && strncmp(hdr.member, VBUS_DISP_DESTROY_WINDOW, 48) == 0) {
            handle_destroy_window(&hdr, &payload.destroy);
        }
        processed++;
    }
    return processed;
}

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

    // Determine the visible intersection of the window with the screen.
    uint32_t dst_x0 = w->x + w->content_x;
    uint32_t dst_y0 = w->y + w->content_y;
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
// src_pixels: ARGB pixel array, w*h entries.
// origin_x/y: top-left corner of the cursor on screen (already hotspot-adjusted).
static void backbuf_blit_cursor_pixels(
    const uint32_t* src_pixels, uint32_t w, uint32_t h, int32_t origin_x, int32_t origin_y
) {
    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;

    for (uint32_t row = 0; row < h; row++) {
        int32_t py = origin_y + (int32_t)row;
        if (py < 0 || (uint32_t)py >= sh) continue;

        for (uint32_t col = 0; col < w; col++) {
            int32_t px = origin_x + (int32_t)col;
            if (px < 0 || (uint32_t)px >= sw) continue;

            uint32_t c = src_pixels[row * w + col];
            uint8_t a = (uint8_t)(c >> 24);
            if (a == 0) continue;  // fully transparent — skip

            if (a == 0xFF) {
                // Fully opaque — direct write, no blending needed.
                g_comp.backbuf[(uint32_t)py * sw + (uint32_t)px] = c;
            } else {
                // Semi-transparent — blend over background.
                uint32_t bg = g_comp.backbuf[(uint32_t)py * sw + (uint32_t)px];
                uint32_t inv = 255u - a;
                uint8_t r = (uint8_t)(((c >> 16 & 0xFF) * a + (bg >> 16 & 0xFF) * inv) / 255);
                uint8_t g = (uint8_t)(((c >> 8 & 0xFF) * a + (bg >> 8 & 0xFF) * inv) / 255);
                uint8_t b = (uint8_t)(((c & 0xFF) * a + (bg & 0xFF) * inv) / 255);
                g_comp.backbuf[(uint32_t)py * sw + (uint32_t)px] =
                    0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
        }
    }
}

// Draw the cursor into the backbuffer.
// Uses the XCursor image when available; falls back to the built-in 16x16 bitmap.
// The hotspot (xhot/yhot) is subtracted so that the logical pointer position
// matches the visual tip of the cursor.
static void backbuf_draw_cursor(void) {
    if (g_xcursor_ok) {
        int32_t origin_x = g_comp.mx - (int32_t)g_xcursor.xhot;
        int32_t origin_y = g_comp.my - (int32_t)g_xcursor.yhot;
        backbuf_blit_cursor_pixels(g_xcursor.pixels, g_xcursor.width, g_xcursor.height, origin_x, origin_y);
    } else {
        // Fallback: built-in 16x16 bitmap, hotspot is implicitly (0,0).
        backbuf_blit_cursor_pixels(g_cursor_pixels, 16, 16, g_comp.mx, g_comp.my);
    }
}

static int get_glyph_index(uint32_t unicode) {
    for (int i = 0; i < 4; i++) {
        const lv_font_fmt_txt_cmap_t* cmap = &cmaps[i];
        if (unicode >= cmap->range_start && unicode < (cmap->range_start + cmap->range_length)) {
            return unicode - cmap->range_start + cmap->glyph_id_start;
        }
    }
    return 0;
}

static void ssd_draw_glyph(uint32_t px, uint32_t py, char ch, uint32_t color) {
    uint32_t code = (uint8_t)ch;
    int idx = get_glyph_index(code);
    const lv_font_fmt_txt_glyph_dsc_t* dsc = &glyph_dsc[idx];

    if (dsc->box_w == 0 || dsc->box_h == 0) return;

    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;

    // Montserrat 12px Annahme (Passe line_height und base_line an deine Datei an!)
    const int32_t font_line_height = 15;
    const int32_t font_base_line = 3;

    int32_t start_y = (int32_t)py + (font_line_height - font_base_line) - dsc->box_h - dsc->ofs_y;
    int32_t start_x = (int32_t)px + dsc->ofs_x;

    // Textfarbe in ihre ARGB-Bestandteile zerlegen
    uint8_t fg_r = (color >> 16) & 0xFF;
    uint8_t fg_g = (color >> 8) & 0xFF;
    uint8_t fg_b = color & 0xFF;

    // Bei 4 bpp repräsentiert jeder Index 4 Bits (0.5 Bytes)
    uint32_t bit_ptr = dsc->bitmap_index * 8;

    for (uint16_t row = 0; row < dsc->box_h; row++) {
        int32_t current_y = start_y + row;

        for (uint16_t col = 0; col < dsc->box_w; col++) {
            int32_t current_x = start_x + col;

            // 4-Bit Wert (0 bis 15) aus dem Byte-Stream extrahieren
            uint8_t byte_val = glyph_bitmap[bit_ptr / 8];
            uint8_t alpha4;
            if ((bit_ptr % 8) == 0) {
                alpha4 = (byte_val >> 4) & 0x0F;  // Obere 4 Bits (erstes Pixel)
            } else {
                alpha4 = byte_val & 0x0F;  // Untere 4 Bits (zweites Pixel)
            }
            bit_ptr += 4;  // 4 Bits weitergehen für das nächste Pixel

            // Wenn das Pixel eine Sichtbarkeit hat und im sichtbaren Bereich liegt
            if (alpha4 > 0 && current_y >= 0 && (uint32_t)current_y < sh && current_x >= 0 &&
                (uint32_t)current_x < sw) {
                uint32_t buf_idx = (uint32_t)current_y * sw + (uint32_t)current_x;

                if (alpha4 == 15) {
                    // Voll deckendes Pixel -> Einfach überschreiben (optimiert)
                    g_comp.backbuf[buf_idx] = color;
                } else {
                    // Alpha-Blending: Skaliere 0-15 auf den Bereich 0-255
                    uint32_t alpha = (alpha4 * 255) / 15;
                    uint32_t inv_alpha = 255 - alpha;

                    // Hintergrundfarbe aus dem Backbuffer lesen
                    uint32_t bg_color = g_comp.backbuf[buf_idx];
                    uint8_t bg_r = (bg_color >> 16) & 0xFF;
                    uint8_t bg_g = (bg_color >> 8) & 0xFF;
                    uint8_t bg_b = bg_color & 0xFF;

                    // Lineare Interpolation (Mischung)
                    uint8_t out_r = (fg_r * alpha + bg_r * inv_alpha) / 255;
                    uint8_t out_g = (fg_g * alpha + bg_g * inv_alpha) / 255;
                    uint8_t out_b = (fg_b * alpha + bg_b * inv_alpha) / 255;

                    // Pixel zurückschreiben
                    g_comp.backbuf[buf_idx] = (0xFF000000) | (out_r << 16) | (out_g << 8) | out_b;
                }
            }
        }
    }
}

// Measure the pixel width of a string using the 5×7 font (5 px per glyph + 1 px gap).
static uint32_t ssd_text_width(const char* s) {
    if (!s || !*s) return 0;
    uint32_t total_width = 0;

    while (*s) {
        uint32_t code = (uint8_t)*s;  // Adjust for UTF-8/umlauts later if necessary
        int idx = get_glyph_index(code);

        total_width += (glyph_dsc[idx].adv_w + 8) / 16;
        s++;
    }
    return total_width;
}

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

// Blit a 16×16 ARGB icon array centred at (cx, cy) into the backbuffer.
// Reuses backbuf_blit_cursor_pixels() which already handles clipping + alpha blending.
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

// Returns which titlebar button (x,y) hits: 0=close, 1=maximize, 2=minimize, -1=none.
// w's btn_* bounding boxes must already be populated (done by ssd_draw_decorations).
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

// Draw a horizontal line in the backbuffer, clipped to screen.
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

// Draw a vertical line in the backbuffer, clipped to screen.
static void ssd_vline(uint32_t x, uint32_t y0, uint32_t len, uint32_t color) {
    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;
    if (x >= sw) return;
    uint32_t y1 = y0 + len;
    if (y1 > sh) y1 = sh;
    for (uint32_t y = y0; y < y1; y++) g_comp.backbuf[y * sw + x] = color;
}

// Fill a rectangle in the backbuffer, clipped to screen.
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

    // Outer rectangle of the entire decorated window (content + decorations).
    const uint32_t ox = w->x;
    const uint32_t oy = w->y;
    const uint32_t ow = w->w;
    const uint32_t oh = w->h;
    const uint32_t tb = (uint32_t)cfg->ssd_titlebar_h;

    ssd_fill_rect(ox, oy, ow, tb, cfg->ssd_color_titlebar);

    // --- Titlebar buttons (right side: close → max → min) ---
    const uint32_t btn_d = (uint32_t)cfg->ssd_btn_size;    // button diameter
    const uint32_t btn_r = btn_d / 2;                      // radius
    const uint32_t btn_m = (uint32_t)cfg->ssd_btn_margin;  // gap between buttons
    const uint32_t btn_rp = (uint32_t)cfg->ssd_btn_right_pad;

    // Vertical center of button in titlebar
    const uint32_t btn_cy = oy + tb / 2;

    // Right-to-left: close is rightmost
    const uint32_t close_cx = ox + ow - btn_rp - btn_r;
    const uint32_t max_cx = close_cx - btn_d - btn_m;
    const uint32_t min_cx = max_cx - btn_d - btn_m;

    // Determine hover colors
    bool hover_this = (g_comp.hover_btn_window == w);
    bool pressed_this = (g_comp.pressed_btn_window == w);
    uint32_t col_close = cfg->ssd_color_btn_close;
    uint32_t col_max = cfg->ssd_color_btn_maximize;
    uint32_t col_min = cfg->ssd_color_btn_minimize;
    if (pressed_this && g_comp.pressed_btn_idx == 0) col_close = color_lighten(col_close, 160);
    if (pressed_this && g_comp.pressed_btn_idx == 1) col_max = color_lighten(col_max, 160);
    if (pressed_this && g_comp.pressed_btn_idx == 2) col_min = color_lighten(col_min, 160);
    if (!pressed_this && hover_this && g_comp.hover_btn_idx == 0) col_close = color_lighten(col_close, 60);
    if (!pressed_this && hover_this && g_comp.hover_btn_idx == 1) col_max = color_lighten(col_max, 60);
    if (!pressed_this && hover_this && g_comp.hover_btn_idx == 2) col_min = color_lighten(col_min, 60);

    // Draw circles
    ssd_fill_circle_aa(close_cx, btn_cy, btn_r, col_close);
    ssd_fill_circle_aa(max_cx, btn_cy, btn_r, col_max);
    ssd_fill_circle_aa(min_cx, btn_cy, btn_r, col_min);

    // Draw icons inside circles (arm/half ~= radius * 0.4, minimum 2)
    ssd_blit_icon_16(close_cx, btn_cy, window_close_symbolic_16px);
    ssd_blit_icon_16(max_cx, btn_cy, window_maximize_symbolic_16px);
    ssd_blit_icon_16(min_cx, btn_cy, window_minimize_symbolic_16px);

    // Store hit-boxes (square bounding box around each circle)
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
    // Clamp the center-point so it doesn't overlap the buttons on the right.
    int32_t title_x = (int32_t)ox + ((int32_t)ow - (int32_t)text_w) / 2;
    const uint32_t title_y = oy + (tb - 15) / 2;
    if (title_x < (int32_t)ox) title_x = (int32_t)ox;

    const char* p = w->title;
    uint32_t gx = (uint32_t)title_x;
    while (*p) {
        const int idx = get_glyph_index((uint8_t)*p);
        ssd_draw_glyph(gx, title_y, *p, cfg->ssd_color_title_fg);
        gx += (glyph_dsc[idx].adv_w + 8) / 16;
        p++;
    }

    // --- Borders ---
    // Bottom edge of titlebar
    ssd_hline(ox, oy + tb - 1, ow, cfg->ssd_color_border);
    // Left border (full window height)
    ssd_vline(ox, oy, oh, cfg->ssd_color_border);
    // Right border
    if (ow >= 1) ssd_vline(ox + ow - 1, oy, oh, cfg->ssd_color_border);
    // Bottom border
    if (oh >= 1) ssd_hline(ox, oy + oh - 1, ow, cfg->ssd_color_border);
}

static void composite_frame(void) {
    backbuf_clear();

    for (int i = 0; i < MAX_WINDOWS; i++) {
        crep_window_t* w = &g_comp.windows[i];
        if (!w->active || !w->pixels) continue;
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

static crep_window_t* find_window_at(int32_t x, int32_t y) {
    // MAX_WINDOWS-1 downwards, to hit the topmost window in the Z-index first
    for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
        crep_window_t* w = &g_comp.windows[i];
        if (!w->active || !w->pixels) continue;

        if (x >= (int32_t)w->x && x < (int32_t)(w->x + w->w) && y >= (int32_t)w->y && y < (int32_t)(w->y + w->h)) {
            return w;
        }
    }
    return NULL;
}

static bool is_titlebar_hit(const crep_window_t* w, int32_t x, int32_t y) {
    if (w->fullscreen) return false;
    return x >= (int32_t)w->x && x < (int32_t)(w->x + w->w) && y >= (int32_t)w->y &&
           y < (int32_t)(w->y + w->content_y);  // content_y == titlebar_h
}

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

            // Hover-State aktualisieren
            crep_window_t* prev_hover_win = g_comp.hover_btn_window;
            int prev_hover_idx = g_comp.hover_btn_idx;
            g_comp.hover_btn_window = NULL;
            g_comp.hover_btn_idx = -1;

            for (int wi = MAX_WINDOWS - 1; wi >= 0; wi--) {
                crep_window_t* w = &g_comp.windows[wi];
                if (!w->active || !w->pixels || w->fullscreen) continue;
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

            // Drag während Move direkt aktualisieren
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
                continue;  // kein Client-Event während Drag
            }

            // Coalescing: letztes Move-Event im Batch merken
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

        // Button-Events (Press/Release)

        uint8_t pressed = (uint8_t)(ev->buttons & ~g_comp.last_buttons);
        uint8_t released = (uint8_t)(~ev->buttons & g_comp.last_buttons);
        g_comp.last_buttons = ev->buttons;

        if ((pressed & 1) && !g_comp.drag_window) {
            crep_window_t* w = find_window_at(g_comp.mx, g_comp.my);
            if (w && is_titlebar_hit(w, g_comp.mx, g_comp.my)) {
                int btn = hit_test_titlebar_buttons(w, g_comp.mx, g_comp.my);
                if (btn >= 0) {
                    // Merken — erst beim Release auslösen
                    g_comp.pressed_btn_window = w;
                    g_comp.pressed_btn_idx = btn;
                    g_comp.needs_present = true;
                    continue;
                }
                // Kein Button getroffen → Drag starten
                g_comp.drag_window = w;
                g_comp.drag_grab_x = g_comp.mx - (int32_t)w->x;
                g_comp.drag_grab_y = g_comp.my - (int32_t)w->y;
                continue;
            }
        }

        // Button-Release: Aktion nur auslösen wenn Maus noch über demselben Button
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
                        handle_destroy_window(NULL, (vbus_display_destroy_window_t*)&req);
                    }
                    if (btn == 1) { /* Maximize placeholder */
                    }
                    if (btn == 2) { /* Minimize placeholder */
                    }
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

        // Button-Event an Client weiterleiten
        crep_window_t* target_win = find_window_at(g_comp.mx, g_comp.my);
        if (target_win) {
            int32_t local_x = g_comp.mx - (int32_t)(target_win->x + target_win->content_x);
            int32_t local_y = g_comp.my - (int32_t)(target_win->y + target_win->content_y);

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
                get_realm_id(),
                target_win->owner_realm_id,
                &input_payload,
                sizeof(input_payload)
            );
        }
    }

    // Einmal pro Batch das gecoalescte Move-Event senden
    if (coalesced_win) {
        vbus_signal_to(
            VBUS_IFACE_DISPLAY,
            VBUS_DISP_INPUT_EVENT,
            get_realm_id(),
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
    cfg->ssd_color_border = (uint32_t)lua_get_table_int(L, "ssd", "color_border", 0xFF3A3A52);
    cfg->ssd_color_title_fg = (uint32_t)lua_get_table_int(L, "ssd", "color_title_fg", 0xFFD0D0E8);
    cfg->ssd_color_btn_close = (uint32_t)lua_get_table_int(L, "ssd", "color_btn_close", 0xFFED8796);
    cfg->ssd_color_btn_maximize = (uint32_t)lua_get_table_int(L, "ssd", "color_btn_maximize", 0xFF8EC994);
    cfg->ssd_color_btn_minimize = (uint32_t)lua_get_table_int(L, "ssd", "color_btn_minimize", 0xFFF8D080);
    cfg->ssd_btn_size = (int)lua_get_table_int(L, "ssd", "btn_size", 16);
    cfg->ssd_btn_margin = (int)lua_get_table_int(L, "ssd", "btn_margin", 4);
    cfg->ssd_btn_right_pad = (int)lua_get_table_int(L, "ssd", "btn_right_pad", 8);

    int64_t target_cursor_size = lua_get_table_int(L, "cursor", "xcursor_target_size", 24);
    if (target_cursor_size <= 0) {
        cfg->cursor_xcursor_target_size = 24;
    } else {
        cfg->cursor_xcursor_target_size = target_cursor_size;
    }

    lua_get_table_string(
        L,
        "cursor",
        "xcursor_path",
        cfg->cursor_xcursor_path,
        sizeof(cfg->cursor_xcursor_path),
        "/usr/share/icons/Bibata-Modern-Ice/cursors/left_ptr"
    );

    lua_get_table_string(
        L,
        "compositor",
        "desktop_binary",
        cfg->compositor_desktop_binary,
        sizeof(cfg->compositor_desktop_binary),
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
    else
        printf("Crepusculum: desktop spawned (realm %lld)\n", (int64_t)rid);

    composite_frame();
    printf("Crepusculum: entering compositor loop (%d fps cap)\n", g_comp.display_cfg.target_fps);
    const uint32_t frame_ns = (1000000000LL / g_comp.display_cfg.target_fps);

    int64_t next_frame = now_ns() + frame_ns;

    while (true) {
        const bool mouse_moved = process_mouse();
        drain_vbus();

        if (mouse_moved) g_comp.needs_present = true;

        if (g_comp.needs_present) {
            composite_frame();
        }

        const int64_t remaining = next_frame - now_ns();
        sleep_ns(remaining);
        next_frame += frame_ns;
    }

    return 0;
}