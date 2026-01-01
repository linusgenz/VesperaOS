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
#ifndef VESPERAOS_FRAMEBUFFER_IOCTL_H
#define VESPERAOS_FRAMEBUFFER_IOCTL_H

#include <stdint.h>

/**
 * @brief Describes the framebuffer's display properties and capabilities.
 */
typedef struct
{
    uint32_t width; ///< Screen width in pixels
    uint32_t height; ///< Screen height in pixels
    uint32_t bpp; ///< Bytes per pixel
    uint32_t pitch;
    ///< Bytes per scanline. There is no guarantee that bytes per scanline will correspond to “(width * bytes_per_pixel)”.
    uint32_t is_primary; ///< 1 if this is the primary display, 0 otherwise
} fb_info;

/**
 * @brief Defines a filled rectangle to be drawn on the framebuffer.
 *
 * The rectangle is filled with a solid color specified in ARGB format.
 */
typedef struct
{
    uint32_t x; ///< X coordinate of top-left corner
    uint32_t y; ///< Y coordinate of top-left corner
    uint32_t width; ///< Width of the rectangle in pixels
    uint32_t height; ///< Height of the rectangle in pixels
    uint32_t color; ///< Fill color in ARGB format (0xAARRGGBB)
} fb_rect;

/**
 * @brief Defines a rectangle outline (border) to be drawn on the framebuffer.
 *
 * Only the border of the rectangle is drawn, with configurable thickness.
 */
typedef struct
{
    uint32_t x; ///< X coordinate of top-left corner
    uint32_t y; ///< Y coordinate of top-left corner
    uint32_t width; ///< Width of the rectangle in pixels
    uint32_t height; ///< Height of the rectangle in pixels
    uint32_t color; ///< Border color in ARGB format (0xAARRGGBB)
    uint32_t thickness; ///< Border thickness in pixels
} fb_rect_outline;

/**
 * @brief Specifies a color to clear the entire screen with.
 */
typedef struct
{
    uint32_t color; ///< Clear color in ARGB format (0xAARRGGBB)
} fb_clear;

/**
 * @brief Describes a pixel buffer transfer (blit) operation.
 *
 * Copies a rectangular buffer of pixels from userspace to the framebuffer.
 * The entire buffer is rendered at the specified screen position.
 */
typedef struct {
    const void* pixels;    ///< Pointer to pixel data in ARGB format (0xAARRGGBB)
    uint32_t buffer_width;  ///< Width of the pixel buffer
    uint32_t buffer_height; ///< Height of the pixel buffer
    uint32_t dst_x;        ///< X coordinate on screen to render the buffer
    uint32_t dst_y;        ///< Y coordinate on screen to render the buffer
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
 * Pass a pointer to uint32_t to receive the device ID.
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
#endif //VESPERAOS_FRAMEBUFFER_IOCTL_H
