// stella_icons.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 05.06.26.
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

#ifndef VESPERAOS_USERSPACE_STELLA_ICONS_H
#define VESPERAOS_USERSPACE_STELLA_ICONS_H

#include <stdint.h>

#define STELLA_IMAGE_HEADER_MAGIC (0x19)

/**
 * LVGL v8 lv_img_dsc_t layout — mirrored here so callers need not include
 * <lvgl/lvgl.h>.  The bit-field order must match the LVGL v8 header exactly.
 *
 * cf values used in VesperaOS:
 *   4 = LV_IMG_CF_TRUE_COLOR        (lv_color32_t per pixel, alpha in MSB)
 *   5 = LV_IMG_CF_TRUE_COLOR_ALPHA  (lv_color_t + separate alpha byte; 16-bit only)
 */
typedef struct {
    struct {
        uint32_t magic: 8;          /**< Magic number.*/
        uint32_t cf : 8;            /**< Color format: See `lv_color_format_t`*/
        uint32_t flags: 16;         /**< Image flags, see `lv_image_flags_t`*/

        uint32_t w: 16;
        uint32_t h: 16;
        uint32_t stride: 16;        /**< Number of bytes in a row*/
        uint32_t reserved_2: 16;    /**< Reserved to be used later*/
    } header;
    uint32_t       data_size;
    const uint8_t *data;
    const void * reserved;      /**< A reserved field to make it has same size as lv_draw_buf_t*/
    const void * reserved_2;    /**< A reserved field to make it has same size as lv_draw_buf_t*/
} stella_img_dsc_t;

#endif // VESPERAOS_USERSPACE_STELLA_ICONS_H