// glyph_cache.h
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
#ifndef VESPERAOS_GLYPH_CACHE_H
#define VESPERAOS_GLYPH_CACHE_H

#include <vespera/mm/memory.h>
#include <vespera/types.h>

static constexpr usize GLYPH_CACHE_SIZE = 512;

struct GlyphCacheKey {
    u32 codepoint;
    u32 fg;
    u32 bg;

    bool operator==(const GlyphCacheKey& o) const {
        return codepoint == o.codepoint && fg == o.fg && bg == o.bg;
    }
};

struct GlyphCacheEntry {
    GlyphCacheKey key{};
    u32* pixels{nullptr};  // ARGB bitmap, pre-blended
    u32 width{};
    u32 height{};
    u32 lru_tick{};
    bool valid{false};
};

class GlyphCache {
   public:
    GlyphCache()
        : entries_(static_cast<GlyphCacheEntry*>(kernel::memory::malloc(sizeof(GlyphCacheEntry) * GLYPH_CACHE_SIZE))) {
        for (usize i = 0; i < GLYPH_CACHE_SIZE; i++) {
            entries_[i] = {};
        }
    }

    ~GlyphCache() {
        for (usize i = 0; i < GLYPH_CACHE_SIZE; i++) {
            if (entries_[i].valid && entries_[i].pixels) kernel::memory::free(entries_[i].pixels);
        }
        kernel::memory::free(entries_);
    }

    const GlyphCacheEntry* find(const GlyphCacheKey& key) {
        for (usize i = 0; i < GLYPH_CACHE_SIZE; i++) {
            if (entries_[i].valid && entries_[i].key == key) {
                entries_[i].lru_tick = ++tick_;
                return &entries_[i];
            }
        }
        return nullptr;
    }

    // Eintrag speichern — pixels-Pointer wird übernommen (ownership)
    void insert(const GlyphCacheKey& key, u32* pixels, u32 w, u32 h) {
        // LRU-Slot finden
        usize slot = 0;
        u32 oldest = entries_[0].lru_tick;
        for (usize i = 1; i < GLYPH_CACHE_SIZE; i++) {
            if (!entries_[i].valid) {
                slot = i;
                goto found;
            }
            if (entries_[i].lru_tick < oldest) {
                oldest = entries_[i].lru_tick;
                slot = i;
            }
        }
    found:
        if (entries_[slot].valid && entries_[slot].pixels) kernel::memory::free(entries_[slot].pixels);

        entries_[slot].key = key;
        entries_[slot].pixels = pixels;
        entries_[slot].width = w;
        entries_[slot].height = h;
        entries_[slot].lru_tick = ++tick_;
        entries_[slot].valid = true;
    }

    void invalidate_all() const {
        for (usize i = 0; i < GLYPH_CACHE_SIZE; i++) {
            if (entries_[i].valid && entries_[i].pixels) kernel::memory::free(entries_[i].pixels);
            entries_[i] = {};
        }
    }

   private:
    GlyphCacheEntry* entries_{};
    u32 tick_{0};
};

#endif  // VESPERAOS_GLYPH_CACHE_H
