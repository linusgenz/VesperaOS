// colors.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.03.26.
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
#ifndef VESPERAOS_COLORS_H
#define VESPERAOS_COLORS_H

#include <vespera/types.h>

constexpr u32 BLACK = 0x00000000;
constexpr u32 WHITE = 0x00FFFFFF;
constexpr u32 RED = 0x00FF0000;
constexpr u32 GREEN = 0x0000FF00;
constexpr u32 BLUE = 0x000000FF;
constexpr u32 YELLOW = 0x00FFFF00;
constexpr u32 CYAN = 0x0000FFFF;
constexpr u32 MAGENTA = 0x00FF00FF;
constexpr u32 ORANGE = 0x0000A5FF;
constexpr u32 GRAY = 0x00808080;
constexpr u32 BG_COLOUR = 0x00061220;

#endif  // VESPERAOS_COLORS_H
