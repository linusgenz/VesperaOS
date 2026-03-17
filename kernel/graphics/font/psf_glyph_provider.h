// psf_glyph_provider.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.03.26.
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
#ifndef VESPERAOS_PSF_GLYPH_PROVIDER_H
#define VESPERAOS_PSF_GLYPH_PROVIDER_H

#include "iglyph_provider.h"
#include <vespera/graphics.h>

class PsfGlyphProvider final : public IGlyphProvider {
public:
    explicit PsfGlyphProvider(font_t* font) : font_(font) {
        glyph_.bitmap   = static_cast<u8*>(kernel::memory::malloc(font->width * font->height));
        glyph_.width    = font->width;
        glyph_.height   = font->height;
        glyph_.bearing_x = 0;
        glyph_.bearing_y = font->height;
        glyph_.advance  = font->width;
    }

    ~PsfGlyphProvider() override {
        kernel::memory::free(glyph_.bitmap);
    }

    const RenderedGlyph* get_glyph(u32 codepoint) override {
        if (codepoint >= 128) codepoint = '?';
        const char* src = static_cast<char*>(font_->glyph_buffer)
                          + codepoint * font_->charsize;

        // PSF-Bits → Alpha-Bytes (0 oder 255)
        const u32 bytes_per_row = (font_->width + 7) / 8;
        for (u32 row = 0; row < font_->height; row++) {
            for (u32 col = 0; col < font_->width; col++) {
                u32 byte = col / 8;
                u32 bit  = 7 - (col % 8);
                bool set = (src[row * bytes_per_row + byte] >> bit) & 1;
                glyph_.bitmap[row * font_->width + col] = set ? 255 : 0;
            }
        }
        return &glyph_;
    }

    u32 line_height()   const override { return font_->height; }
    u32 baseline()      const override { return font_->height; }
    u32 char_width()    const override { return font_->width;  }
    bool is_monospace() const override { return true; }

private:
    font_t*       font_;
    RenderedGlyph glyph_{};
};

#endif  // VESPERAOS_PSF_GLYPH_PROVIDER_H
