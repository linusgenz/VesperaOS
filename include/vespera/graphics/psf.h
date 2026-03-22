// psf.h
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
#ifndef VESPERAOS_PSF_H
#define VESPERAOS_PSF_H

#include <vespera/types.h>

#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

typedef struct {
    unsigned char magic[2];
    unsigned char mode;
    unsigned char charsize;
} psf1_header_t;

struct PsfFont {
    void* header;       // PSF1_HEADER* or PSF2_HEADER*
    void* glyph_buffer;
    u32 type;      // 1 = PSF1, 2 = PSF2
    u32 width;
    u32 height;
    u32 charsize;
} ;

#define PSF2_MAGIC 0x864ab572

typedef struct {
    u32 magic;        // 0x864ab572
    u32 version;      // 0
    u32 headersize;   // offset of bitmaps in file
    u32 flags;        // 0 = keine Unicode Tabelle
    u32 length;       // Anzahl der Glyphen
    u32 charsize;     // Bytes pro Glyph
    u32 height;       // Pixelhöhe
    u32 width;        // Pixelbreite
} psf2_header_t;

#endif  // VESPERAOS_PSF_H
