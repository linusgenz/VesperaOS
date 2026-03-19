// utf.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 17.03.26.
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

#include <klib/decoding.h>

bool utf8_decode(utf8_state_t *s, const u8 byte, u32 *out) {
    if (s->remaining == 0) {
        if (byte < 0x80) {
            *out = byte;
            return true;
        }

        if ((byte & 0xE0) == 0xC0) {
            s->codepoint = byte & 0x1F;
            s->remaining = 1;
            return false;
        }

        if ((byte & 0xF0) == 0xE0) {
            s->codepoint = byte & 0x0F;
            s->remaining = 2;
            return false;
        }

        if ((byte & 0xF8) == 0xF0) {
            s->codepoint = byte & 0x07;
            s->remaining = 3;
            return false;
        }

        *out = 0xFFFD;
        return true;
    }
    if ((byte & 0xC0) != 0x80) {
        s->remaining = 0;
        *out = 0xFFFD;
        return true;
    }

    s->codepoint = (s->codepoint << 6) | (byte & 0x3F);
    s->remaining--;

    if (s->remaining == 0) {
        *out = s->codepoint;
        return true;
    }

    return false;
}