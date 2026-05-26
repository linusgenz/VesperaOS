/**
 * @file framebuffer_ioctl.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 01.01.26.
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
#ifndef VESPERAOS_IOCTL_FRAMEBUFFER_H
#define VESPERAOS_IOCTL_FRAMEBUFFER_H

#include <vespera/types.h>
#include <vespera/ioctl.h>

/**
 * @brief Describes the framebuffer's display properties and capabilities.
 */
typedef struct fb_info
{
    u32 width; ///< Screen width in pixels
    u32 height; ///< Screen height in pixels
    u32 bpp; ///< Bytes per pixel
    u32 pitch;
    ///< Bytes per scanline. There is no guarantee that bytes per scanline will correspond to “(width * bytes_per_pixel)”.
    u32 is_primary; ///< 1 if this is the primary display, 0 otherwise
} fb_info_t;

/**
 * @brief Defines a filled rectangle to be drawn on the framebuffer.
 *
 * The rectangle is filled with a solid color specified in ARGB format.
 */
typedef struct fb_rect
{
    u32 x; ///< X coordinate of top-left corner
    u32 y; ///< Y coordinate of top-left corner
    u32 width; ///< Width of the rectangle in pixels
    u32 height; ///< Height of the rectangle in pixels
    u32 color; ///< Fill color in ARGB format (0xAARRGGBB)
} fb_rect_t;

/**
 * @brief Defines a rectangle outline (border) to be drawn on the framebuffer.
 *
 * Only the border of the rectangle is drawn, with configurable thickness.
 */
typedef struct fb_rect_outline
{
    u32 x; ///< X coordinate of top-left corner
    u32 y; ///< Y coordinate of top-left corner
    u32 width; ///< Width of the rectangle in pixels
    u32 height; ///< Height of the rectangle in pixels
    u32 color; ///< Border color in ARGB format (0xAARRGGBB)
    u32 thickness; ///< Border thickness in pixels
} fb_rect_outline_t;

/**
 * @brief Specifies a color to clear the entire screen with.
 */
typedef struct fb_clear
{
    u32 color; ///< Clear color in ARGB format (0xAARRGGBB)
} fb_clear_t;

/**
 * @brief Describes a rectangular pixel buffer transfer (blit) operation.
 *
 * Copies a rectangular region from a source pixel buffer in userspace
 * to the framebuffer.
 *
 * All pixels are expected to use ARGB format (0xAARRGGBB).
 */
typedef struct fb_blit {
    /**
     * @brief Pointer to the top-left pixel of the full source buffer.
     */
    const void* pixels;

    /**
     * @brief Width of the full source buffer in pixels.
     *
     * Also acts as the source stride.
     */
    u32 src_stride;

    /**
     * @brief Height of the full source buffer in pixels.
     */
    u32 src_height;

    /**
     * @brief X coordinate of the source rectangle inside the source buffer.
     */
    u32 src_x;

    /**
     * @brief Y coordinate of the source rectangle inside the source buffer.
     */
    u32 src_y;

    /**
     * @brief Width of the region to copy in pixels.
     */
    u32 width;

    /**
     * @brief Height of the region to copy in pixels.
     */
    u32 height;

    /**
     * @brief X coordinate on the framebuffer where the region is rendered.
     */
    u32 dst_x;

    /**
     * @brief Y coordinate on the framebuffer where the region is rendered.
     */
    u32 dst_y;

} fb_blit_t;

/**
 * @brief IOCTL code to retrieve framebuffer information.
 *
 * Pass a pointer to fb_info struct to receive display properties.
 */
#define FB_IOCTL_GET_INFO           IOR('F', 0x00, fb_info_t)

/**
 * @brief IOCTL code to get the kernel device ID of the backing device.
 *
 * Pass a pointer to u32 to receive the device ID.
 */
#define FB_IOCTL_GET_BACKING_DEVID  IOR('F', 0x01, uint32_t)

/**
 * @brief IOCTL code to draw a filled rectangle.
 *
 * Pass a pointer to fb_rect struct with rectangle parameters.
 */
#define FB_IOCTL_FILL_RECT          IOW('F', 0x02, fb_rect_t)

/**
 * @brief IOCTL code to draw a rectangle outline (border only).
 *
 * Pass a pointer to fb_rect_outline struct with border parameters.
 */
#define FB_IOCTL_DRAW_RECT          IOW('F', 0x03, fb_rect_outline_t)

/**
 * @brief IOCTL code to clear the entire screen with a solid color.
 *
 * Pass a pointer to fb_clear struct with the clear color.
 */
#define FB_IOCTL_CLEAR              IOW('F', 0x04, fb_clear_t)

/**
 * @brief IOCTL code to copy a pixel buffer to the framebuffer.
 *
 * Pass a pointer to fb_blit struct containing the buffer and destination.
 * The buffer must contain ARGB pixels (0xAARRGGBB format).
 */
#define FB_IOCTL_BLIT               IOW('F', 0x05, fb_blit_t)

#define FB_IOCTL_PRESENT               IO('F', 0x06)

#endif //VESPERAOS_IOCTL_FRAMEBUFFER_H
