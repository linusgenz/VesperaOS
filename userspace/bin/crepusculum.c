#include <crepusculum_protocol.h>
#include <realm.h>
#include <stdbool.h>
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

#include "vespera/handles.h"

#define MAX_WINDOWS 16
#define MICE_BATCH 32
#define BG_COLOR 0xFF1A1A24

#define TARGET_FPS 120
#define FRAME_NS (1000000000LL / TARGET_FPS)

#define DESKTOP_BINARY "/bin/firmament"

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
// Built once in init_cursor_pixels(), never rebuilt.
static uint32_t g_cursor_pixels[16 * 16];

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
}

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
    uint32_t flags;

    RealmID owner_realm_id;

    bool dirty;
} crep_window_t;

typedef struct compositor_state {
    HANDLE fb;
    HANDLE mouse;
    fb_info_t info;

    crep_window_t windows[MAX_WINDOWS];
    uint32_t window_count;
    uint32_t next_window_id;

    int32_t mx;
    int32_t my;

    bool needs_present;

    // Software backbuffer: width * height * 4 bytes.
    // All compositing happens here; a single blit pushes it to the screen.
    uint32_t* backbuf;
} compositor_state_t;

static compositor_state_t g_comp;

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
    const uint32_t fb_size = w->w * w->h * CREP_BPP;

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

    for (uint32_t i = 0; i < w->w * w->h; i++) w->pixels[i] = BG_COLOR;

    // Publish geometry; ready=1 is the release barrier for the client.
    w->sync->width = w->w;
    w->sync->height = w->h;
    w->sync->bpp = CREP_BPP;
    w->sync->pitch = w->w * CREP_BPP;
    w->sync->dirty = 0;
    w->sync->seq = 0;
    __atomic_store_n(&w->sync->magic, (uint32_t)CREP_MAGIC, __ATOMIC_RELEASE);
    __atomic_store_n(&w->sync->ready, 1u, __ATOMIC_RELEASE);

    return 0;
}

static void window_free_shm(crep_window_t* w) {
    if (w->sync && w->sync != MAP_FAILED) munmap(w->sync, sizeof(crep_sync_t));
    if (w->pixels && w->pixels != (void*)MAP_FAILED) munmap(w->pixels, w->w * w->h * CREP_BPP);
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

    bool fullscreen = (req->flags & VBUS_DISP_FLAG_FULLSCREEN) || (!req->width || !req->height);
    w->x = 0;
    w->y = 0;
    w->w = fullscreen ? g_comp.info.width : req->width;
    w->h = fullscreen ? g_comp.info.height : req->height;

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
    resp.width = w->w;
    resp.height = w->h;
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
    for (uint32_t i = 0; i < total; i++) g_comp.backbuf[i] = BG_COLOR;
}

// Copy a window's pixel buffer into the backbuffer, clipped to screen bounds.
// Windows are assumed fully opaque; no alpha blending needed here.
static void backbuf_blit_window(const crep_window_t* w) {
    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;

    // Determine the visible intersection of the window with the screen.
    uint32_t dst_x0 = w->x < sw ? w->x : sw;
    uint32_t dst_y0 = w->y < sh ? w->y : sh;
    uint32_t dst_x1 = (w->x + w->w) < sw ? (w->x + w->w) : sw;
    uint32_t dst_y1 = (w->y + w->h) < sh ? (w->y + w->h) : sh;

    if (dst_x0 >= dst_x1 || dst_y0 >= dst_y1) return;

    for (uint32_t row = dst_y0; row < dst_y1; row++) {
        uint32_t src_row = row - w->y;
        const uint32_t* src = &w->pixels[src_row * w->w + (dst_x0 - w->x)];
        uint32_t* dst = &g_comp.backbuf[row * sw + dst_x0];
        memcpy(dst, src, (dst_x1 - dst_x0) * sizeof(uint32_t));
    }
}

// Alpha-blend the pre-baked cursor into the backbuffer.
// Transparent cursor pixels (alpha == 0) are skipped; opaque ones overwrite.
// This handles clipping at screen edges automatically.
static void backbuf_draw_cursor(void) {
    const uint32_t sw = g_comp.info.width;
    const uint32_t sh = g_comp.info.height;

    for (int row = 0; row < 16; row++) {
        int py = g_comp.my + row;
        if (py < 0 || (uint32_t)py >= sh) continue;

        for (int col = 0; col < 16; col++) {
            int px = g_comp.mx + col;
            if (px < 0 || (uint32_t)px >= sw) continue;

            uint32_t c = g_cursor_pixels[row * 16 + col];
            if ((c >> 24) == 0) continue;  // transparent

            g_comp.backbuf[(uint32_t)py * sw + (uint32_t)px] = c;
        }
    }
}

static void composite_frame(void) {
    backbuf_clear();

    for (int i = 0; i < MAX_WINDOWS; i++) {
        crep_window_t* w = &g_comp.windows[i];
        if (!w->active || !w->pixels) continue;
        backbuf_blit_window(w);
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

static bool process_mouse(void) {
    mice_event events[MICE_BATCH];
    ssize_t bytes = read(g_comp.mouse, events, sizeof(events));
    if (bytes <= 0 || (size_t)bytes < sizeof(mice_event)) return false;

    size_t count = (size_t)bytes / sizeof(mice_event);
    bool moved = false;

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
        }

        crep_window_t* target_win = find_window_at(g_comp.mx, g_comp.my);
        if (target_win) {
            int32_t local_x = g_comp.mx - target_win->x;
            int32_t local_y = g_comp.my - target_win->y;

            vbus_display_input_event_t input_payload = {
                .window_id = target_win->id,
                .local_x = local_x,
                .local_y = local_y,
                .buttons = ev->buttons,
                .type = (uint32_t)ev->type
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
    return moved;
}

static inline long long now_ns(void) {
    timespec_t ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void sleep_ns(long long ns) {
    if (ns <= 0) return;
    timespec_t ts = {
        .tv_sec = ns / 1000000000LL,
        .tv_nsec = ns % 1000000000LL,
    };
    nanosleep(&ts, NULL);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    memset(&g_comp, 0, sizeof(g_comp));
    g_comp.next_window_id = 1;

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

    uint32_t backbuf_size = g_comp.info.width * g_comp.info.height * sizeof(uint32_t);
    g_comp.backbuf = (uint32_t*)mmap(NULL, backbuf_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_comp.backbuf == MAP_FAILED) {
        printf("Crepusculum: backbuffer allocation failed\n");
        return 1;
    }

    init_cursor_pixels();

    fb_clear_t clr = {.color = BG_COLOR};
    ioctl(g_comp.fb, FB_IOCTL_CLEAR, &clr);
    ioctl(g_comp.fb, FB_IOCTL_PRESENT, NULL);

    if (vbus_subscribe(VBUS_IFACE_DISPLAY, "") < 0) {
        printf("Crepusculum: vbus_subscribe failed\n");
        return 1;
    }
    printf("Crepusculum: subscribed to %s\n", VBUS_IFACE_DISPLAY);

    g_comp.mx = (int32_t)(g_comp.info.width / 2);
    g_comp.my = (int32_t)(g_comp.info.height / 2);

    const char* desktop_argv[] = {DESKTOP_BINARY, NULL};
    const char* desktop_envp[] = {"PATH=/bin", "TERM=tty0", NULL};
    int64_t rid = spawn_realm(DESKTOP_BINARY, (char**)desktop_argv, (char**)desktop_envp, NULL);
    if (rid < 0)
        printf("Crepusculum: desktop spawn failed (%lld), running without desktop\n", (long long)rid);
    else
        printf("Crepusculum: desktop spawned (realm %lld)\n", (long long)rid);

    composite_frame();
    printf("Crepusculum: entering compositor loop (%d fps cap)\n", TARGET_FPS);

    long long next_frame = now_ns() + FRAME_NS;

    while (true) {
        bool mouse_moved = process_mouse();
        drain_vbus();

        if (mouse_moved) g_comp.needs_present = true;

        if (g_comp.needs_present) {
            composite_frame();
        }

        long long remaining = next_frame - now_ns();
        sleep_ns(remaining);
        next_frame += FRAME_NS;
    }

    return 0;
}