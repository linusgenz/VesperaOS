// ttf_glyph_provider.h
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
#ifndef VESPERAOS_TTF_GLYPH_PROVIDER_H
#define VESPERAOS_TTF_GLYPH_PROVIDER_H

#include "iglyph_provider.h"
#include <vespera/mm/memory.h>

struct stbtt_fontinfo;

class TtfGlyphProvider final : public IGlyphProvider {
public:
    // font_data: the raw TTF file content (remains in the caller's possession,
    //            must remain valid for the duration of the provider's lifetime)
    // size_px:  font size in pixels
    TtfGlyphProvider(const u8* font_data, usize font_size, float size_px);
    ~TtfGlyphProvider() override;

    const RenderedGlyph* get_glyph(u32 codepoint) override;

    [[nodiscard]] u32  line_height()   const override { return line_height_; }
    [[nodiscard]] u32  baseline()      const override { return baseline_;    }
    [[nodiscard]] u32  char_width()    const override { return char_width_;  }
    [[nodiscard]] bool is_monospace()  const override { return true;         }

    [[nodiscard]] bool is_valid() const { return valid_; }

private:
    stbtt_fontinfo* info_{};
    float           scale_{};
    u32             line_height_{};
    u32             baseline_{};
    u32             char_width_{};
    bool            valid_{false};

    // Puffer für eine einzelne gerenderte Glyph
    u8*           bitmap_buf_{};
    u32           bitmap_buf_size_{};
    RenderedGlyph current_{};
};

#endif  // VESPERAOS_TTF_GLYPH_PROVIDER_H
