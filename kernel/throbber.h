// throbber.h
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

#ifndef VESPERAOS_THROBBER_H
#define VESPERAOS_THROBBER_H

#include <vespera/types.h>

#define THROBBER_SIZE 64
#define THROBBER_FRAMES 8
#define THROBBER_SEGMENTS 8
#define THROBBER_RADIUS 14
#define THROBBER_THICKNESS 4

#define SEGMENT_COUNT 64
#define TRAIL_LENGTH 16

#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_PI_4_P_0273 1.05839816339744830962

inline u32 throbber_frames[SEGMENT_COUNT][THROBBER_SIZE * THROBBER_SIZE];
inline u8 segment_map[THROBBER_SIZE * THROBBER_SIZE];
inline u8 mask_map[THROBBER_SIZE * THROBBER_SIZE];

void generate_throbber();
void render_throbber(void *arg);

#endif  // VESPERAOS_THROBBER_H