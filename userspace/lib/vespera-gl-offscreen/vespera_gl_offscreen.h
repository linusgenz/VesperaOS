// vespera_gl_offscreen.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.08.26.
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

#ifndef VESPERA_GL_OFFSCREEN_H
#define VESPERA_GL_OFFSCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "frontend/api.h"
#include "util/xmlconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pipe_screen;
struct pipe_context;
struct pipe_resource;
struct st_context;

struct vespera_gl_offscreen {
    int drm_fd;

    struct pipe_screen *screen;
    struct pipe_context *pipe;

    /* Eigene Einbettung von pipe_frontend_screen/-drawable -- siehe
     * api.h Kopfkommentar: "This is inherited by a screen/drawable in
     * the DRI/GLX/WGL frontends". Wir sind das Frontend hier. */
    struct pipe_frontend_screen fscreen;
    struct pipe_frontend_drawable drawable;
    struct st_visual visual;

    struct st_context *st;

    struct pipe_resource *back_buffer;

    uint32_t width;
    uint32_t height;

    driOptionCache option_info;
    driOptionCache option_cache;
};

/* Oeffnet /dev/dri/card0, baut Screen/Context/State-Tracker-Context auf,
 * legt einen ${width}x${height} RGBA8-Backbuffer an und macht den
 * resultierenden GL-Context fuer den aufrufenden Thread "current".
 * Gibt bei Erfolg true zurueck. */
bool vespera_gl_offscreen_create(struct vespera_gl_offscreen *ctx,
                                  uint32_t width, uint32_t height);

/* Liest den aktuellen Inhalt des Backbuffers via transfer_map zurueck und
 * schreibt ihn als binaeres PPM (P6) nach 'path'. Muss nach den GL-Aufrufen,
 * aber vor destroy() erfolgen. */
bool vespera_gl_offscreen_write_ppm(struct vespera_gl_offscreen *ctx,
                                     const char *path);

void vespera_gl_offscreen_destroy(struct vespera_gl_offscreen *ctx);

#ifdef __cplusplus
}
#endif

#endif /* VESPERA_GL_OFFSCREEN_H */