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
typedef struct
{
    u32 x; ///< X coordinate of top-left corner
    u32 y; ///< Y coordinate of top-left corner
    u32 width; ///< Width of the rectangle in pixels
    u32 height; ///< Height of the rectangle in pixels
    u32 color; ///< Border color in ARGB format (0xAARRGGBB)
    u32 thickness; ///< Border thickness in pixels
} fb_rect_outline;

/**
 * @brief Specifies a color to clear the entire screen with.
 */
typedef struct
{
    u32 color; ///< Clear color in ARGB format (0xAARRGGBB)
} fb_clear;

/**
 * @brief Describes a pixel buffer transfer (blit) operation.
 *
 * Copies a rectangular buffer of pixels from userspace to the framebuffer.
 * The entire buffer is rendered at the specified screen position.
 */
typedef struct {
    const void* pixels;    ///< Pointer to pixel data in ARGB format (0xAARRGGBB)
    u32 buffer_width;  ///< Width of the pixel buffer
    u32 buffer_height; ///< Height of the pixel buffer
    u32 dst_x;        ///< X coordinate on screen to render the buffer
    u32 dst_y;        ///< Y coordinate on screen to render the buffer
} fb_blit;

/**
 * @brief IOCTL code to retrieve framebuffer information.
 *
 * Pass a pointer to fb_info struct to receive display properties.
 */
#define FB_IOCTL_GET_INFO           0x4600

/**
 * @brief IOCTL code to get the kernel device ID of the backing device.
 *
 * Pass a pointer to u32 to receive the device ID.
 */
#define FB_IOCTL_GET_BACKING_DEVID  0x4601

/**
 * @brief IOCTL code to draw a filled rectangle.
 *
 * Pass a pointer to fb_rect struct with rectangle parameters.
 */
#define FB_IOCTL_FILL_RECT          0x4602

/**
 * @brief IOCTL code to draw a rectangle outline (border only).
 *
 * Pass a pointer to fb_rect_outline struct with border parameters.
 */
#define FB_IOCTL_DRAW_RECT          0x4603

/**
 * @brief IOCTL code to clear the entire screen with a solid color.
 *
 * Pass a pointer to fb_clear struct with the clear color.
 */
#define FB_IOCTL_CLEAR              0x4604

/**
 * @brief IOCTL code to copy a pixel buffer to the framebuffer.
 *
 * Pass a pointer to fb_blit struct containing the buffer and destination.
 * The buffer must contain ARGB pixels (0xAARRGGBB format).
 */
#define FB_IOCTL_BLIT               0x4605
#endif //VESPERAOS_IOCTL_FRAMEBUFFER_H
