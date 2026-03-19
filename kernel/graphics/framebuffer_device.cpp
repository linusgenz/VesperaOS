/**
 * @file framebuffer_device.cpp
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 30.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
 */

#include "framebuffer_device.h"

#include <vespera_errno.h>

int FramebufferDevice::open(CharFile**) {
    return 0;
}

int FramebufferDevice::release(CharFile*) {
    return 0;
}

isize FramebufferDevice::read(CharFile* /*cf*/, void* /*buffer*/, usize /*count*/, usize /*offset*/) {
    return -EUNSUPPORTED;
}

isize FramebufferDevice::write(CharFile* /*cf*/, const void* /*buffer*/, usize /*count*/) {
    return -EUNSUPPORTED;
}

int FramebufferDevice::ioctl(CharFile*, const u32 cmd, void* arg) {
    if (!arg && cmd != FB_IOCTL_GET_INFO && cmd != FB_IOCTL_GET_BACKING_DEVID) {
        return -EINVAL;
    }

    auto [drv, kd] = DisplayManager::primary();
    if (!drv) return -ENODEV;

    switch (cmd) {
        case FB_IOCTL_GET_INFO: {
            if (!arg) return -EINVAL;
            auto* info = static_cast<FbInfo*>(arg);
            info->width = drv->screen_width_px();
            info->height = drv->screen_height_px();
            info->bpp = 4;
            info->pitch = drv->bytes_per_scanline();
            info->is_primary = 1;
            return 0;
        }

        case FB_IOCTL_GET_BACKING_DEVID: {
            if (!arg) return -EINVAL;
            *static_cast<u32*>(arg) = kd ? kd->id : 0;
            return 0;
        }

        case FB_IOCTL_FILL_RECT:
            return fill_rect(static_cast<const FbRect*>(arg));

        case FB_IOCTL_DRAW_RECT:
            return draw_rect_outline(static_cast<const FbRectOutline*>(arg));

        case FB_IOCTL_CLEAR:
            return clear_screen(static_cast<const FbClear*>(arg));

        case FB_IOCTL_BLIT:
            return blit_pixels(static_cast<const FbBlit*>(arg));

        default:
            return -ENOTTY;
    }
}

bool FramebufferDevice::validate_rect(const u32 x, const u32 y, const u32 w, const u32 h) {
    auto backend = DisplayManager::primary();
    if (!backend.drv) return false;

    const u32 screen_w = backend.drv->screen_width_px();
    const u32 screen_h = backend.drv->screen_height_px();

    // Check for overflow and bounds
    if (w == 0 || h == 0) return false;
    if (x >= screen_w || y >= screen_h) return false;
    if (x + w > screen_w || y + h > screen_h) return false;

    return true;
}

bool FramebufferDevice::validate_blit(const FbBlit* blit) {
    if (!blit->pixels) return false;
    if (blit->buffer_width == 0 || blit->buffer_height == 0) return false;

    return validate_rect(blit->dst_x, blit->dst_y, blit->buffer_width, blit->buffer_height);
}

int FramebufferDevice::fill_rect(const FbRect* rect) {
    if (!validate_rect(rect->x, rect->y, rect->width, rect->height)) {
        return -EINVAL;
    }

    auto backend = DisplayManager::primary();
    if (!backend.drv->fill_rect(rect->x, rect->y, rect->width, rect->height, rect->color)) {
        return -EIO;
    }

    return 0;
}

int FramebufferDevice::draw_rect_outline(const FbRectOutline* rect) {
    if (rect->thickness == 0) return -EINVAL;
    if (!validate_rect(rect->x, rect->y, rect->width, rect->height)) {
        return -EINVAL;
    }

    auto backend = DisplayManager::primary();
    auto* drv = backend.drv;

    if (!drv->fill_rect(rect->x, rect->y, rect->width, rect->thickness, rect->color)) return -EIO;

    // Bottom edge
    if (!drv->fill_rect(rect->x, rect->y + rect->height - rect->thickness, rect->width, rect->thickness, rect->color))
        return -EIO;

    if (rect->height > 2 * rect->thickness) {
        if (!drv->fill_rect(
                rect->x, rect->y + rect->thickness, rect->thickness, rect->height - 2 * rect->thickness, rect->color
            ))
            return -EIO;
    }

    if (rect->height > 2 * rect->thickness) {
        if (!drv->fill_rect(
                rect->x + rect->width - rect->thickness,
                rect->y + rect->thickness,
                rect->thickness,
                rect->height - 2 * rect->thickness,
                rect->color
            ))
            return -EIO;
    }

    return 0;
}

int FramebufferDevice::clear_screen(const FbClear* clear) {
    auto backend = DisplayManager::primary();
    if (!backend.drv->fill_rect(0, 0, backend.drv->screen_width_px(), backend.drv->screen_height_px(), clear->color)) {
        return -EIO;
    }

    return 0;
}

int FramebufferDevice::blit_pixels(const FbBlit* blit) {
    if (!validate_blit(blit)) return -EINVAL;

    auto backend = DisplayManager::primary();

    if (!backend.drv->blit_buffer(blit->pixels, blit->buffer_width, blit->buffer_height, blit->dst_x, blit->dst_y)) {
        return -EIO;
    }

    return 0;
}
