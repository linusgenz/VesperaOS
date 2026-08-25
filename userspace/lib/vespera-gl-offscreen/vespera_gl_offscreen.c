// vespera_gl_offscreen.c
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

#include "vespera_gl_offscreen.h"

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "util/format/u_formats.h"
#include "frontend/api.h"
#include "main/menums.h"
#include "state_tracker/st_context.h"
#include "state_tracker/st_manager.h"
#include "util/xmlconfig.h"
#include "util/driconf.h"

#define VESPERA_DRM_DEVICE "/dev/dri/card0"

static const driOptionDescription gallium_driconf[] = {
#include "gallium/auxiliary/pipe-loader/driinfo_gallium.h"
};


static const driOptionDescription iris_driconf[] = {
#include "gallium/drivers/iris/driinfo_iris.h"
};

static driOptionDescription *
vespera_merge_driconf(unsigned *merged_count)
{
    unsigned gallium_count = ARRAY_SIZE(gallium_driconf);
    unsigned iris_count = ARRAY_SIZE(iris_driconf);
    driOptionDescription *merged =
        malloc((gallium_count + iris_count) * sizeof(*merged));
    memcpy(merged, gallium_driconf, sizeof(*merged) * gallium_count);
    memcpy(&merged[gallium_count], iris_driconf, sizeof(*merged) * iris_count);
    *merged_count = gallium_count + iris_count;
    return merged;
}


/* Aus gallium/winsys/iris/drm -- siehe eure winsys-Portierung. */
extern struct pipe_screen *iris_drm_screen_create(int fd,
        const struct pipe_screen_config *config);

/* -------------------------------------------------------------------------
 * pipe_frontend_drawable Callbacks
 *
 * validate() liefert bei jedem Aufruf denselben, in vespera_gl_offscreen_
 * create() einmalig angelegten Backbuffer zurueck (siehe Kopfkommentar in
 * vespera_gl_offscreen.h). Kein dynamisches Resize noetig fuer den Test.
 * ---------------------------------------------------------------------- */

static bool
vespera_drawable_validate(struct st_context *st,
                           struct pipe_frontend_drawable *drawable,
                           const enum st_attachment_type *statts,
                           unsigned count,
                           struct pipe_resource **out,
                           struct pipe_resource **resolve)
{
    struct vespera_gl_offscreen *ctx =
            (struct vespera_gl_offscreen *)((char *)drawable -
                    offsetof(struct vespera_gl_offscreen, drawable));

    for (unsigned i = 0; i < count; i++) {
        if (statts[i] == ST_ATTACHMENT_BACK_LEFT ||
            statts[i] == ST_ATTACHMENT_FRONT_LEFT) {
            /* pipe_resource_reference-Semantik: Aufrufer erwartet eine
             * referenzierte Kopie. Da wir keine echte Refcount-Bibliothek
             * hier einbinden wollen, geben wir den Zeiger direkt zurueck --
             * ctx haelt die eigentliche Lebenszeit-Referenz in destroy(). */
            out[i] = ctx->back_buffer;
        } else {
            out[i] = NULL;
        }
    }

    if (resolve) {
        for (unsigned i = 0; i < count; i++)
            resolve[i] = NULL;
    }

    return true;
}

static bool
vespera_drawable_flush_front(struct st_context *st,
                              struct pipe_frontend_drawable *drawable,
                              enum st_attachment_type statt)
{
    /* Kein Fenstersystem angebunden -- nichts zu tun. Auslesen passiert
     * explizit ueber vespera_gl_offscreen_write_ppm(). */
    return true;
}

static bool
vespera_drawable_flush_swapbuffers(struct st_context *st,
                                    struct pipe_frontend_drawable *drawable)
{
    return true;
}

/* -------------------------------------------------------------------------
 * pipe_frontend_screen Callbacks -- alle optional, bis auf 'screen' selbst.
 * ---------------------------------------------------------------------- */

static int
vespera_fscreen_get_param(struct pipe_frontend_screen *fscreen,
                           enum st_manager_param param)
{
    return 0;
}

bool
vespera_gl_offscreen_create(struct vespera_gl_offscreen *ctx,
                             uint32_t width, uint32_t height)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->width = width;
    ctx->height = height;

    ctx->drm_fd = open(VESPERA_DRM_DEVICE, O_RDONLY);
    if (ctx->drm_fd < 0) {
        fprintf(stderr, "vespera_gl_offscreen: open(%s) failed. errno=%u %s\n",
                VESPERA_DRM_DEVICE, errno, strerror(errno));
        return false;
    }

    unsigned merged_count;
    driOptionDescription *merged = vespera_merge_driconf(&merged_count);
    memset(&ctx->option_info, 0, sizeof(ctx->option_info));
    memset(&ctx->option_cache, 0, sizeof(ctx->option_cache));
    driParseOptionInfo(&ctx->option_info, merged, merged_count);
    free(merged);
    driParseConfigFiles(&ctx->option_cache, &ctx->option_info,
                         &(driConfigFileParseParams) { .driverName = "iris" });

    struct pipe_screen_config conf = {};
    conf.options = &ctx->option_cache;
    conf.options_info = &ctx->option_info;
    ctx->screen = iris_drm_screen_create(ctx->drm_fd, &conf);
    if (!ctx->screen) {
        fprintf(stderr, "vespera_gl_offscreen: iris_drm_screen_create failed\n");
        close(ctx->drm_fd);
        return false;
    }

    /* 3. pipe_frontend_screen einbetten --------------------------------
     * Wir SIND das Frontend hier (kein DRI/GLX/WGL zwischengeschaltet).
     * Siehe api.h Kopfkommentar auf pipe_frontend_screen. */
    ctx->fscreen.screen = ctx->screen;
    ctx->fscreen.get_egl_image = NULL;
    ctx->fscreen.validate_egl_image = NULL;
    ctx->fscreen.get_param = vespera_fscreen_get_param;
    ctx->fscreen.set_background_context = NULL;
    ctx->fscreen.st_screen = NULL;

    /* 4. Backbuffer anlegen ---------------------------------------------
     * RGBA8, als Render-Target + fuer transfer_map(READ) nutzbar. */
    struct pipe_resource templ;
    memset(&templ, 0, sizeof(templ));
    templ.target = PIPE_TEXTURE_2D;
    templ.format = PIPE_FORMAT_R8G8B8A8_UNORM;
    templ.width0 = width;
    templ.height0 = height;
    templ.depth0 = 1;
    templ.array_size = 1;
    templ.last_level = 0;
    templ.usage = PIPE_USAGE_DEFAULT;
    templ.bind = PIPE_BIND_RENDER_TARGET | PIPE_BIND_DISPLAY_TARGET;

    ctx->back_buffer = ctx->screen->resource_create(ctx->screen, &templ);
    if (!ctx->back_buffer) {
        fprintf(stderr, "vespera_gl_offscreen: resource_create failed\n");
        goto fail_screen;
    }

    /* 5. st_visual -------------------------------------------------------
     * Nur ein Farbpuffer, kein Depth/Stencil fuer den ersten Test. */
    ctx->visual.buffer_mask = ST_ATTACHMENT_BACK_LEFT_MASK;
    ctx->visual.color_format = PIPE_FORMAT_R8G8B8A8_UNORM;
    ctx->visual.depth_stencil_format = PIPE_FORMAT_NONE;
    ctx->visual.accum_format = PIPE_FORMAT_NONE;
    ctx->visual.samples = 0;

    /* 6. pipe_frontend_drawable einbetten -------------------------------- */
    ctx->drawable.stamp = 1;
    ctx->drawable.ID = 0;
    ctx->drawable.fscreen = &ctx->fscreen;
    ctx->drawable.visual = &ctx->visual;
    ctx->drawable.flush_front = vespera_drawable_flush_front;
    ctx->drawable.validate = vespera_drawable_validate;
    ctx->drawable.flush_swapbuffers = vespera_drawable_flush_swapbuffers;

    /* 7. GL-Context ueber den State-Tracker erzeugen --------------------- */
    struct st_context_attribs attribs;
    memset(&attribs, 0, sizeof(attribs));
    attribs.profile = API_OPENGL_COMPAT; /* gl_api Enum, siehe mtypes.h */
    attribs.major = 2;
    attribs.minor = 1;
    attribs.flags = 0;
    attribs.context_flags = 0;
    attribs.visual = ctx->visual;
    /* options bleibt komplett auf 0/false -- Default-Verhalten. */

    enum st_context_error st_error = ST_CONTEXT_SUCCESS;
    ctx->st = st_api_create_context(&ctx->fscreen, &attribs, &st_error, NULL);
    if (!ctx->st || st_error != ST_CONTEXT_SUCCESS) {
        fprintf(stderr, "vespera_gl_offscreen: st_api_create_context failed (error=%d)\n",
                (int)st_error);
        goto fail_buffer;
    }

    /* 8. Context "current" machen ----------------------------------------
     * Read und Draw-Drawable sind hier identisch (kein Front/Back-Swap-
     * Fenstersystem). */
    if (!st_api_make_current(ctx->st, &ctx->drawable, &ctx->drawable)) {
        fprintf(stderr, "vespera_gl_offscreen: st_api_make_current failed\n");
        goto fail_context;
    }

    return true;

fail_context:
    st_destroy_context(ctx->st);
fail_buffer:
    ctx->screen->resource_destroy(ctx->screen, ctx->back_buffer);
fail_screen:
    ctx->screen->destroy(ctx->screen);
    close(ctx->drm_fd);
    return false;
}

bool
vespera_gl_offscreen_write_ppm(struct vespera_gl_offscreen *ctx,
                                const char *path)
{
    struct pipe_context *pipe = ctx->st->pipe;
    struct pipe_transfer *transfer = NULL;

    struct pipe_box box;
    box.x = 0;
    box.y = 0;
    box.z = 0;
    box.width = ctx->width;
    box.height = ctx->height;
    box.depth = 1;

    void *map = pipe->texture_map(pipe, ctx->back_buffer, 0, PIPE_MAP_READ,
                                   &box, &transfer);
    if (!map) {
        fprintf(stderr, "vespera_gl_offscreen: texture_map failed\n");
        return false;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "vespera_gl_offscreen: fopen(%s) failed\n", path);
        pipe->texture_unmap(pipe, transfer);
        return false;
    }

    fprintf(f, "P6\n%u %u\n255\n", ctx->width, ctx->height);

    const uint8_t *row_base = (const uint8_t *)map;
    for (uint32_t y = 0; y < ctx->height; y++) {
        const uint8_t *row = row_base + (size_t)y * transfer->stride;
        for (uint32_t x = 0; x < ctx->width; x++) {
            /* RGBA8 -> PPM will RGB, Alpha wird verworfen. */
            fputc(row[x * 4 + 0], f);
            fputc(row[x * 4 + 1], f);
            fputc(row[x * 4 + 2], f);
        }
    }

    fclose(f);
    pipe->texture_unmap(pipe, transfer);
    return true;
}

void
vespera_gl_offscreen_destroy(struct vespera_gl_offscreen *ctx)
{
    if (ctx->st)
        st_destroy_context(ctx->st);
    if (ctx->back_buffer && ctx->screen)
        ctx->screen->resource_destroy(ctx->screen, ctx->back_buffer);
    if (ctx->screen)
        ctx->screen->destroy(ctx->screen);
    if (ctx->drm_fd >= 0)
        close(ctx->drm_fd);
    memset(ctx, 0, sizeof(*ctx));
}