/**
 * @file gpu_blt.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 16.12.25.
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
#ifndef VESPERAOS_GPU_BLT_H
#define VESPERAOS_GPU_BLT_H

#include <cstdint>

struct BltRect {
    uint32_t x, y;
    uint32_t width, height;
};

class GpuBltDriver {
public:
    virtual bool rect(BltRect rect, uint32_t color) = 0;
    virtual void copy(BltRect src, BltRect dst) = 0;
    virtual uint32_t get_width() = 0;
    virtual uint32_t get_height() = 0;
    virtual bool draw_str(const char* str, uint32_t x, uint32_t y, uint32_t colour, uint32_t bg) = 0;
    virtual bool scroll(uint32_t scroll_pixels) = 0;
    virtual ~GpuBltDriver() = default;
};

extern GpuBltDriver* gpu_blt;

#endif //VESPERAOS_GPU_BLT_H