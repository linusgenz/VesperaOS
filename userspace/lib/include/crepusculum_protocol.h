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

#define CREP_SYNC_SHM_NAME   "/crep_sync"
#define CREP_FB_SHM_NAME     "/crep_fb"

#define CREP_MAGIC  0x43524550UL   // "CREP"
#define CREP_BPP    4              // bytes per pixel, ARGB8888

typedef struct crep_sync {
    // written by server, read by client
    volatile uint32_t magic;    // CREP_MAGIC once server is ready
    volatile uint32_t ready;    // 1 after server has initialized the FB SHM

    // geometry – constant after ready = 1
    uint32_t width;             // framebuffer width  in pixels
    uint32_t height;            // framebuffer height in pixels
    uint32_t bpp;               // bytes per pixel (= CREP_BPP = 4)
    uint32_t pitch;             // bytes per row = width * bpp

    // written by client, read by server
    volatile uint32_t dirty;    // client sets to 1 when a new frame is ready
    volatile uint32_t seq;      // client increments each time it finishes a frame

    uint32_t _pad[8];           // reserved, keep struct size at 64 bytes
} crep_sync_t;

// Size of the pixel buffer SHM given a sync structure.
static inline uint32_t crep_fb_size(const crep_sync_t* s) {
    return s->pitch * s->height;
}

#define VBUS_DISP_SET_STRUT  "SetStrut"

typedef enum {
    CREP_STRUT_TOP    = 0,
    CREP_STRUT_BOTTOM = 1,
    CREP_STRUT_LEFT   = 2,
    CREP_STRUT_RIGHT  = 3,
} crep_strut_edge_t;

typedef struct {
    crep_strut_edge_t edge;
    uint32_t size;    // in px
    uint32_t _pad[2];
} vbus_display_set_strut_t;

#define VBUS_DISP_WINDOW_CONFIGURE  "WindowConfigure"

typedef struct {
    uint32_t window_id;
    uint32_t width;
    uint32_t height;
    uint32_t _pad;
} vbus_display_configure_t;

#endif // CREPUSCULUM_PROTOCOL_H
