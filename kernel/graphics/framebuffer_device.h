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

#include <vespera/devices/char_device.h>

#include "display_manager.h"

#define FB_IOCTL_GET_INFO 0x4600
#define FB_IOCTL_GET_BACKING_DEVID 0x4601
#define FB_IOCTL_FILL_RECT 0x4602
#define FB_IOCTL_DRAW_RECT 0x4603
#define FB_IOCTL_CLEAR 0x4604
#define FB_IOCTL_BLIT 0x4605

struct FbInfo {
    u32 width;
    u32 height;
    u32 bpp;
    u32 pitch;
    u32 is_primary;  // 1 = yes, 0 = no
};

struct FbRect {
    u32 x;
    u32 y;
    u32 width;
    u32 height;
    u32 color;  // ARGB format: 0xAARRGGBB
};

struct FbRectOutline {
    u32 x;
    u32 y;
    u32 width;
    u32 height;
    u32 color;      // ARGB format
    u32 thickness;  // border thickness in pixels
};

struct FbClear {
    u32 color;  // ARGB format
};

struct FbBlit {
    const void *pixels;
    u32 buffer_width;
    u32 buffer_height;
    u32 dst_x;
    u32 dst_y;
};

class FramebufferDevice final : public CharDevice {
   public:
    FramebufferDevice(const char *name, BusType bus)
        : CharDevice( bus) {
    }

    int open(CharFile **out_cf) override;
    int release(CharFile *cf) override;

    isize read(CharFile *cf, void *buffer, usize count, usize offset) override;
    isize write(CharFile *cf, const void *buffer, usize count) override;

    int ioctl(CharFile *cf, u32 cmd, void *arg) override;

   private:
    // Validation helpers
    bool validate_rect(u32 x, u32 y, u32 w, u32 h);
    bool validate_blit(const FbBlit *blit);

    // Drawing helpers
    int fill_rect(const FbRect *rect);
    int draw_rect_outline(const FbRectOutline *rect);
    int clear_screen(const FbClear *clear);
    int blit_pixels(const FbBlit *blit);
};

#endif  // VESPERAOS_FRAMEBUFFER_DEVICE_H
