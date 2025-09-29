// throbber.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 16.08.25.
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

#include "throbber.h"

#include <basic_renderer.h>
#include <log.h>
#include <scheduling.h>
#include <sys/syscalls.h>

#include "scheduling/cpu_scheduler.h"
#include "include/time.h"

static inline float abs(float v) {
    return v < 0.0f ? -v : v;
}

double atan2(double y,double x)
{
    double absx, absy;
    absy = abs(y);
    absx = abs(x);
    short octant = ((x<0) << 2) + ((y<0) << 1 ) + (absx <= absy);
    switch (octant) {
        case 0: {
            if (x == 0 && y == 0)
                return 0;
            double val = absy/absx;
            return (M_PI_4_P_0273 - 0.273*val)*val; //1st octant
            break;
        }
        case 1:{
            if (x == 0 && y == 0)
                return 0.0;
            double val = absx/absy;
            return M_PI_2 - (M_PI_4_P_0273 - 0.273*val)*val; //2nd octant
            break;
        }
        case 2: {
            double val =absy/absx;
            return -(M_PI_4_P_0273 - 0.273*val)*val; //8th octant
            break;
        }
        case 3: {
            double val =absx/absy;
            return -M_PI_2 + (M_PI_4_P_0273 - 0.273*val)*val;//7th octant
            break;
        }
        case 4: {
            double val =absy/absx;
            return  M_PI - (M_PI_4_P_0273 - 0.273*val)*val;  //4th octant
        }
        case 5: {
            double val =absx/absy;
            return  M_PI_2 + (M_PI_4_P_0273 - 0.273*val)*val;//3rd octant
            break;
        }
        case 6: {
            double val =absy/absx;
            return -M_PI + (M_PI_4_P_0273 - 0.273*val)*val; //5th octant
            break;
        }
        case 7: {
            double val =absx/absy;
            return -M_PI_2 - (M_PI_4_P_0273 - 0.273*val)*val; //6th octant
            break;
        }
        default:
            return 0.0;
    }
}

void generate_throbber() {
    int cx = THROBBER_SIZE / 2;
    int cy = THROBBER_SIZE / 2;
    int rmin2 = (THROBBER_RADIUS - THROBBER_THICKNESS/2) * (THROBBER_RADIUS - THROBBER_THICKNESS/2);
    int rmax2 = (THROBBER_RADIUS + THROBBER_THICKNESS/2) * (THROBBER_RADIUS + THROBBER_THICKNESS/2);

    // Segment-Map vorberechnen
    for (int y = 0; y < THROBBER_SIZE; y++) {
        for (int x = 0; x < THROBBER_SIZE; x++) {
            int dx = x - cx;
            int dy = y - cy;
            int r2 = dx*dx + dy*dy;

            if (r2 < rmin2 || r2 > rmax2) {
                mask_map[y*THROBBER_SIZE + x] = 0;
                continue;
            }
            mask_map[y*THROBBER_SIZE + x] = 1;

            double angle = atan2((double)dy, (double)dx);
            if (angle < 0) angle += 2*M_PI;
            uint8_t ang = (uint8_t)(angle * 255.0 / (2*M_PI));
            segment_map[y*THROBBER_SIZE + x] = (ang * SEGMENT_COUNT) / 256;
        }
    }

    for (int frame = 0; frame < SEGMENT_COUNT; frame++) {
        for (int i = 0; i < THROBBER_SIZE * THROBBER_SIZE; i++) {
            if (!mask_map[i]) {
                throbber_frames[frame][i] = 0x00000000;
                continue;
            }
            uint8_t seg = segment_map[i];
            int diff = (frame - seg + SEGMENT_COUNT) % SEGMENT_COUNT;

            uint32_t bg = 0xFF404040;
            if (diff < TRAIL_LENGTH) {
                uint8_t shade = 255 - (diff * (255 / TRAIL_LENGTH));

                uint8_t bg_r = (bg >> 16) & 0xFF;
                uint8_t bg_g = (bg >> 8) & 0xFF;
                uint8_t bg_b = bg & 0xFF;

                uint8_t r = bg_r + ((255 - bg_r) * shade) / 255;
                uint8_t g = bg_g + ((255 - bg_g) * shade) / 255;
                uint8_t b = bg_b + ((255 - bg_b) * shade) / 255;

                throbber_frames[frame][i] = 0xFF000000 | (r << 16) | (g << 8) | b;
            } else {
                throbber_frames[frame][i] = bg;
            }
        }
    }
}

void clear_throbber(uint32_t x, uint32_t y) {
    Framebuffer* fb = global_renderer->TargetFramebuffer;
    uint32_t bg_color = global_renderer->get_bg_colour();

    for (uint32_t row = 0; row < THROBBER_SIZE; row++) {
        uint32_t* fb_row = (uint32_t*)((uint8_t*)fb->base_address + (y + row) * fb->pixels_per_scanline * 4);
        for (uint32_t col = 0; col < THROBBER_SIZE; col++) {
            fb_row[x + col] = bg_color;
        }
    }
}

void draw_bitmap(Framebuffer *fb, uint32_t *bitmap, uint32_t w, uint32_t h, uint32_t x, uint32_t y) {
    for (uint32_t row = 0; row < h; row++) {
        uint32_t *fb_row = (uint32_t *) ((uint8_t *) fb->base_address + (y + row) * fb->pixels_per_scanline * 4);
        for (uint32_t col = 0; col < w; col++) {
            uint32_t px = bitmap[row * w + col];
            if (px >> 24) {
                // alpha > 0
                fb_row[x + col] = px;
            }
        }
    }
}

void render_throbber(void *arg) {
    uint32_t frame = 0;
    uint32_t draw_x = (global_renderer->TargetFramebuffer->width / 2) - (THROBBER_SIZE / 2);
    uint32_t draw_y = ((global_renderer->TargetFramebuffer->height * 3) / 4) - (THROBBER_SIZE / 2);

    while (true) {
        if (system_initialized) {
            clear_throbber(draw_x, draw_y);
            break;
        }

        draw_bitmap(global_renderer->TargetFramebuffer,
                    throbber_frames[frame],
                    THROBBER_SIZE,
                    THROBBER_SIZE,
                    draw_x,
                    draw_y);

        frame = (frame + 1) % SEGMENT_COUNT;
        kernel::time::sleep_ms(30);
    }

 //   kernel::scheduling::thread_exit();
}