// qwerty.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 30.07.25.
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

#ifndef QWERTY_H
#define QWERTY_H
#include <vespera/types.h>

namespace ps2::keyboard::qwerty {

    constexpr u8 LEFT_SHIFT   = 0x2A;
    constexpr u8 RIGHT_SHIFT  = 0x36;
    constexpr u8 ENTER        = 0x1C;
    constexpr u8 BACKSPACE    = 0x0E;
    constexpr u8 SPACEBAR     = 0x39;
    constexpr u8 LEFT_CTRL    = 0x1D;
    constexpr u8 LEFT_ALT     = 0x38;

    constexpr u8 RIGHT_CTRL   = 0x1D; // E0 1D
    constexpr u8 RIGHT_ALT    = 0x38; // E0 38
    constexpr u8 LEFT_SUPER   = 0x5B; // E0 5B
    constexpr u8 RIGHT_SUPER  = 0x5C; // E0 5C

    char translate(u8 scancode, bool shift);

}

#endif //QWERTY_H
