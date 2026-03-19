// ttf_glyph_provider.cpp
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

#include "ttf_glyph_provider.h"

#include <klib/math/math.h>
#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

// ReSharper disable CppInconsistentNaming

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x, u) kernel::memory::malloc(x)
#define STBTT_free(x, u) kernel::memory::free(x)
#define STBTT_strlen(x) strlen(x)
#define STBTT_memcpy memcpy
#define STBTT_memset memset
#define STBTT_assert(x) ((void)0)
#define STBTT_ifloor(x) ((int)floor(x))
#define STBTT_iceil(x) ((int)ceil(x))
#define STBTT_sqrt(x) sqrt(x)
#define STBTT_pow(x, y) pow((x), (y))
#define STBTT_fabs(x) fabs(x)
#define STBTT_cos(x) cos(x)
#define STBTT_acos(x) acos(x)
#define STBTT_fmod(x, y) fmod((x), (y))
#define NULL nullptr
#include "../../../lib/stb/stb_truetype.h"

TtfGlyphProvider::TtfGlyphProvider(const u8* font_data, usize, const float size_px)
    : info_(static_cast<stbtt_fontinfo*>(kernel::memory::malloc(sizeof(stbtt_fontinfo)))) {
    if (!stbtt_InitFont(info_, font_data, 0)) {
        Log::error("[TTF] stbtt_InitFont failed");
        kernel::memory::free(info_);
        info_ = nullptr;
        return;
    }

    scale_ = stbtt_ScaleForPixelHeight(info_, size_px);

    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(info_, &ascent, &descent, &line_gap);

    baseline_ = static_cast<u32>(lround(static_cast<long double>(ascent) * scale_));

    line_height_ = static_cast<u32>(lround(static_cast<long double>(ascent - descent + line_gap) * scale_));

    // Determine character width based on ‘M’ (typical for monospace fonts)
    int adv = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(info_, 'M', &adv, &lsb);
    char_width_ = static_cast<u32>(lroundf(adv * scale_));

    // Bitmap buffer for the largest expected glyph
    bitmap_buf_size_ = char_width_ * line_height_ * 2;  // etwas Puffer
    bitmap_buf_ = static_cast<u8*>(kernel::memory::malloc(bitmap_buf_size_));

    valid_ = true;
    Log::ok(
        "[TTF] Font loaded: %upx, line=%u, base=%u, charw=%u",
        static_cast<u32>(size_px),
        line_height_,
        baseline_,
        char_width_
    );
}

TtfGlyphProvider::~TtfGlyphProvider() {
    if (bitmap_buf_) kernel::memory::free(bitmap_buf_);
    if (info_) kernel::memory::free(info_);
}

const RenderedGlyph* TtfGlyphProvider::get_glyph(const u32 codepoint) {
    if (!valid_) return nullptr;

    int w = 0, h = 0, off_x = 0, off_y = 0;
    u8* bm = stbtt_GetCodepointBitmap(info_, 0, scale_, static_cast<int>(codepoint), &w, &h, &off_x, &off_y);

    if (!bm) {
        // Invisible character (e.g., space) → return an empty bitmap
        current_.bitmap = bitmap_buf_;
        current_.width = char_width_;
        current_.height = line_height_;
        current_.bearing_x = 0;
        current_.bearing_y = baseline_;
        current_.advance = static_cast<i32>(char_width_);
        if (bitmap_buf_) {
            memset(bitmap_buf_, 0, static_cast<usize>(char_width_) * static_cast<usize>(line_height_));
        }
        return &current_;
    }

    // Make sure the buffer is large enough
    const usize needed = static_cast<usize>(w) * h;
    if (needed > bitmap_buf_size_) {
        kernel::memory::free(bitmap_buf_);
        bitmap_buf_size_ = needed * 2;
        bitmap_buf_ = static_cast<u8*>(kernel::memory::malloc(bitmap_buf_size_));
    }

    memcpy(bitmap_buf_, bm, needed);
    stbtt_FreeBitmap(bm, nullptr);

    int adv = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(info_, static_cast<int>(codepoint), &adv, &lsb);

    current_.bitmap = bitmap_buf_;
    current_.width = static_cast<u32>(w);
    current_.height = static_cast<u32>(h);
    current_.bearing_x = off_x;
    current_.bearing_y = -off_y;  // stb returns a negative off_y value for the top
    current_.advance = static_cast<i32>(lroundf(adv * scale_));
    return &current_;
}