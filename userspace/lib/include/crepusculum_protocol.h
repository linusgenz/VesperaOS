// crepusculum.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 28.05.26.
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

#ifndef CREPUSCULUM_PROTOCOL_H
#define CREPUSCULUM_PROTOCOL_H

#include <stdint.h>
#include <vespera/dev/mice.h>

/* =========================================================================
 * VBUS Interface & Message Identifiers
 * ========================================================================= */

#define VBUS_IFACE_DISPLAY "vespera.display"

/* Window Lifecycle Messages */
#define VBUS_DISP_CREATE_WINDOW "CreateWindow"   /* CALL */
#define VBUS_DISP_WINDOW_CREATED "WindowCreated" /* RETURN */
#define VBUS_DISP_DESTROY_WINDOW "DestroyWindow" /* CALL */
#define VBUS_DISP_WINDOW_OPENED "WindowOpened"   /* SIGNAL */
#define VBUS_DISP_WINDOW_CLOSED "WindowClosed"   /* SIGNAL */

/* Window Management & Topology Messages */
#define VBUS_DISP_WINDOW_CONFIGURE "WindowConfigure" /* SIGNAL */
#define VBUS_DISP_SET_STRUT "SetStrut"               /* SIGNAL */

/* Window State & Input Interaction Messages */
#define VBUS_DISP_WINDOW_ACTIVATE "WindowActivate"   /* SIGNAL */
#define VBUS_DISP_WINDOW_MINIMIZED "WindowMinimized" /* SIGNAL */
#define VBUS_DISP_WINDOW_RESTORED "WindowRestored"   /* SIGNAL */
#define VBUS_DISP_WINDOW_COMMIT "WindowCommit"       /* SIGNAL */
#define VBUS_DISP_INPUT_EVENT "InputEvent"           /* SIGNAL */

/* Window Creation Flags */
#define VBUS_DISP_FLAG_FULLSCREEN (1u << 0) /* Request full-screen placement */
#define VBUS_DISP_FLAG_TOPMOST (1u << 1)    /* Keep on top (panels, overlays) */
#define VBUS_DISP_FLAG_NO_CURSOR (1u << 2)  /* Compositor hides cursor over this window */

/* =========================================================================
 * Shared Memory (SHM) Low-Level Compositor Protocol
 * ========================================================================= */

#define CREP_SYNC_SHM_NAME "/crep_sync"
#define CREP_FB_SHM_NAME "/crep_fb"
#define CREP_MAGIC 0x43524550UL /* "CREP" */
#define CREP_BPP 4              /* ARGB8888 */

typedef struct crep_sync {
    /* Written by server, read by client */
    volatile uint32_t magic; /* CREP_MAGIC when server is ready */
    volatile uint32_t ready; /* 1 after SHM initialization */

    /* Geometry constants (valid after ready == 1) */
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch; /* Bytes per row (width * bpp) */

    /* Written by client, read by server */
    volatile uint32_t dirty; /* Set to 1 when a new frame is ready */
    volatile uint32_t seq;   /* Incremented on every completed frame */

    uint32_t _pad[8]; /* Reserved structural alignment padding */
} crep_sync_t;

static inline uint32_t crep_fb_size(const crep_sync_t* s) {
    return s->pitch * s->height;
}

/* =========================================================================
 * Protocol Data Structures
 * ========================================================================= */

typedef enum {
    CREP_STRUT_TOP = 0,
    CREP_STRUT_BOTTOM = 1,
    CREP_STRUT_LEFT = 2,
    CREP_STRUT_RIGHT = 3,
} crep_strut_edge_t;

typedef struct {
    crep_strut_edge_t edge;
    uint32_t size; /* In pixels */
    uint32_t _pad[2];
} vbus_display_set_strut_t;

typedef struct {
    uint32_t width;  /* 0 = full screen width */
    uint32_t height; /* 0 = full screen height */
    uint32_t flags;  /* VBUS_DISP_FLAG_* */
    uint32_t _pad;
    char title[64];
} vbus_display_create_window_t;

typedef struct {
    uint32_t window_id; /* >0 on success */
    int32_t status;     /* 0 = success, negative errno on failure */
    uint32_t width;
    uint32_t height;
    char sync_shm[64];
    char fb_shm[64];
} vbus_display_window_info_t;

typedef struct {
    uint32_t window_id;
    char title[64];
} vbus_display_window_opened_t;

typedef struct {
    uint32_t window_id;
    uint32_t width;
    uint32_t height;
    uint32_t _pad;
} vbus_display_configure_t;

typedef struct {
    uint32_t window_id;
    uint32_t _pad;
} vbus_display_window_id_t;

typedef struct {
    uint32_t window_id;
    uint32_t _pad;
} vbus_display_destroy_window_t;

typedef struct {
    uint32_t window_id;
    uint32_t seq;
} vbus_display_commit_t;

typedef struct {
    uint32_t window_id;
    mice_event_type type;
    int32_t local_x;
    int32_t local_y;
    uint32_t buttons;      /* Bitmask of mouse button state */
    mice_button button_id; /* Triggering button for click events */
    int32_t scroll;        /* Wheel delta (positive = away from user) */
    uint32_t _pad;
} vbus_display_input_event_t;

/* =========================================================================
 * Global Message Payload Union
 * ========================================================================= */

typedef union {
    vbus_display_create_window_t create_window;
    vbus_display_window_info_t window_info;
    vbus_display_window_opened_t opened;
    vbus_display_configure_t configure;
    vbus_display_set_strut_t set_strut;
    vbus_display_commit_t commit;
    vbus_display_destroy_window_t destroy_window;
    vbus_display_input_event_t input;

    /* Messages sharing the standard window ID layout */
    vbus_display_window_id_t closed;
    vbus_display_window_id_t minimized;
    vbus_display_window_id_t restored;
    vbus_display_window_id_t activate;
} vbus_payload_t;

#endif /* CREPUSCULUM_PROTOCOL_H */