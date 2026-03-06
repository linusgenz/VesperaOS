/**
 * @file framebuffer_device.h
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
#ifndef VESPERAOS_FRAMEBUFFER_DEVICE_H
#define VESPERAOS_FRAMEBUFFER_DEVICE_H

#include <kernel/devices/char_device.h>

#include "../types/handle.h"
#include "display_manager.h"

#define FB_IOCTL_GET_INFO 0x4600
#define FB_IOCTL_GET_BACKING_DEVID 0x4601
#define FB_IOCTL_FILL_RECT 0x4602
#define FB_IOCTL_DRAW_RECT 0x4603
#define FB_IOCTL_CLEAR 0x4604
#define FB_IOCTL_BLIT 0x4605

struct FbInfo {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    uint32_t is_primary;  // 1 = yes, 0 = no
};

struct FbRect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t color;  // ARGB format: 0xAARRGGBB
};

struct FbRectOutline {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t color;      // ARGB format
    uint32_t thickness;  // border thickness in pixels
};

struct FbClear {
    uint32_t color;  // ARGB format
};

struct FbBlit {
    const void *pixels;
    uint32_t buffer_width;
    uint32_t buffer_height;
    uint32_t dst_x;
    uint32_t dst_y;
};

class FramebufferDevice final : public CharDevice {
   public:
    FramebufferDevice(const char *name, BusType bus)
        : CharDevice( bus) {
    }

    int open(CharFile **out_cf) override;
    int release(CharFile *cf) override;

    ssize_t read(CharFile *cf, void *buffer, size_t count, size_t offset) override;
    ssize_t write(CharFile *cf, const void *buffer, size_t count) override;

    int ioctl(CharFile *cf, uint32_t cmd, void *arg) override;

   private:
    // Validation helpers
    bool validate_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    bool validate_blit(const FbBlit *blit);

    // Drawing helpers
    int fill_rect(const FbRect *rect);
    int draw_rect_outline(const FbRectOutline *rect);
    int clear_screen(const FbClear *clear);
    int blit_pixels(const FbBlit *blit);
};

#endif  // VESPERAOS_FRAMEBUFFER_DEVICE_H
