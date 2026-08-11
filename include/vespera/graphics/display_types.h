// display_types.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 11.08.26.
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

#ifndef VESPERAOS_GRAPHICS_DISPLAY_TYPES_H
#define VESPERAOS_GRAPHICS_DISPLAY_TYPES_H

#include <vespera/types.h>

struct Resolution {
    u32 width{0u};
    u32 height{0u};

    constexpr Resolution() = default;

    constexpr Resolution(const u32 w, const u32 h) : width(w), height(h) {
    }

    [[nodiscard]] constexpr uint64_t total_pixels() const noexcept {
        return static_cast<uint64_t>(width) * height;
    }

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return width > 0 && height > 0;
    }

    constexpr bool operator==(const Resolution&) const = default;
    constexpr bool operator!=(const Resolution&) const = default;
};

struct DisplayMode {
    Resolution resolution;
    u32 refresh_rate_mhz{60000}; // in mHz (60000 = 60 Hz)
    u32 bpp{32u};                // Bits per pixel

    [[nodiscard]] constexpr uint32_t refresh_rate_hz() const noexcept {
        return refresh_rate_mhz / 1000;
    }
};

#endif //VESPERAOS_GRAPHICS_DISPLAY_TYPES_H
