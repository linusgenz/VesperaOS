// cursor_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
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

#ifndef VESPERAOS_CURSOR_REGS_H
#define VESPERAOS_CURSOR_REGS_H

#include <vespera/types.h>

/**
 * @brief Cursor Control Register (CUR_CTL).
 *
 * Controls cursor mode/format, 180-degree rotation, gamma correction,
 * pipe CSC, double-buffer update behaviour, and forced alpha blending.
 *
 * The cursor is enabled by programming any valid non-zero mode in
 * @c mode, and disabled by programming @c MODE_DISABLE (0).
 *
 * @note Double-buffered. Shadow registers are armed by a write to
 *       @c CUR_BASE (or when the cursor is not enabled). The update
 *       takes effect at the start of the next vertical blank, or
 *       immediately if the pipe is not enabled.
 *
 * @note 180-degree rotation is only valid for 32 bpp cursor formats;
 *       @c rotate_180 must be 0 when using 2 bpp indexed formats.
 *       Hardware rotates the cursor image only — software must adjust
 *       @c CUR_POS manually to match physical display orientation.
 *
 * @note Force Alpha is only for use with ARGB cursor formats.
 *
 * <b>Register addresses:</b>
 * | Pipe | Address | Power Well |
 * |------|---------|------------|
 * | A    | 0x70080 | PG1        |
 * | B    | 0x71080 | PG2        |
 * | C    | 0x72080 | PG2        |
 *
 * @see IHD-OS-KBL-Vol 2c-1.17, pp. 386–389 (CUR_CTL)
 * @see CUR_BASE for arming updates.
 */
union CUR_CTL {
    enum CursorMode : u32 {
        MODE_DISABLE = 0b000000,               ///< Cursor is disabled
        MODE_128X128_32BPP_AND_INV = 0b000010, ///< 128x128 32bpp AND/INVERT
        MODE_256X256_32BPP_AND_INV = 0b000011, ///< 256x256 32bpp AND/INVERT
        MODE_64X64_2BPP_3COLOR = 0b000100,     ///< 64x64 2bpp Indexed 3-color + transparency
        MODE_64X64_2BPP_2COLOR = 0b000101,     ///< 64x64 2bpp Indexed AND/XOR 2-color
        MODE_64X64_2BPP_4COLOR = 0b000110,     ///< 64x64 2bpp Indexed 4-color
        MODE_64X64_32BPP_AND_INV = 0b000111,   ///< 64x64 32bpp AND/INVERT
        MODE_128X128_32BPP_ARGB = 0b100010,    ///< 128x128 32bpp ARGB (8:8:8:8 MSB-A:R:G:B)
        MODE_256X256_32BPP_ARGB = 0b100011,    ///< 256x256 32bpp ARGB (8:8:8:8 MSB-A:R:G:B)
        MODE_64X64_32BPP_AND_XOR = 0b100100,   ///< 64x64 32bpp AND/XOR
        MODE_128X128_32BPP_AND_XOR = 0b100101, ///< 128x128 32bpp AND/XOR
        MODE_256X256_32BPP_AND_XOR = 0b100110, ///< 256x256 32bpp AND/XOR
        MODE_64X64_32BPP_ARGB = 0b100111,      ///< 64x64 32bpp ARGB (8:8:8:8 MSB-A:R:G:B)
    };

    enum ForceAlphaValue : u32 {
        FORCE_ALPHA_DISABLE = 0b00, ///< Alpha blend normally over any plane
        FORCE_ALPHA_50 = 0b01,      ///< Alpha >= 50% -> opaque, < 50% -> transparent
        FORCE_ALPHA_75 = 0b10,      ///< Alpha >= 75% -> opaque, < 75% -> transparent
        FORCE_ALPHA_100 = 0b11,     ///< Alpha == 100% -> opaque, < 100% -> transparent
    };

    enum ForceAlphaPlane : u32 {
        FORCE_ALPHA_PLANE_DISABLE = 0b00,      ///< Disable alpha forcing
        FORCE_ALPHA_PIPE_CSC_ENABLED = 0b01,   ///< Force alpha where overlapping plane has pipe CSC enabled
        FORCE_ALPHA_PIPE_CSC_DISABLED = 0b10,  ///< Force alpha where overlapping plane has pipe CSC disabled
    };

    struct {
        CursorMode mode : 6;                   ///< [5:0]   Cursor Mode Select (see CursorMode enum)
        u32 reserved6 : 2;                     ///< [7:6]   Reserved
        ForceAlphaValue force_alpha_value : 2; ///< [9:8]   Force Alpha Value (ARGB formats only)
        ForceAlphaPlane force_alpha_plane : 2; ///< [11:10] Force Alpha Plane Select
        u32 reserved12 : 2;                    ///< [13:12] Reserved
        u32 trickle_feed_enable : 1;           ///< [14]    Trickle Feed Enable (0=Enable, 1=Disable) — do not set to 1
        u32 rotate_180 : 1;                    ///< [15]    180 Rotation (0=None, 1=180 deg; 32bpp only)
        u32 reserved16 : 7;                    ///< [22:16] MBZ
        u32 allow_db_update_disable : 1;       ///< [23]    Allow Double Buffer Update Disable (0=Not Allowed, 1=Allowed)
        u32 pipe_csc_enable : 1;               ///< [24]    Pipe CSC Enable
        u32 reserved25 : 1;                    ///< [25]    Reserved
        u32 gamma_enable : 1;                  ///< [26]    Pipe Gamma Enable (bypassed in VGA pop-up)
        u32 reserved27 : 5;                    ///< [31:27] Reserved
    } __attribute__((packed));

    u32 raw;

    [[nodiscard]] constexpr bool enabled() const {
        return mode != MODE_DISABLE;
    }
};

static_assert(sizeof(CUR_CTL) == 4, "CUR_CTL must be 32 bits");

constexpr u32 CUR_CTL_A = 0x70080;
constexpr u32 CUR_CTL_B = 0x71080;
constexpr u32 CUR_CTL_C = 0x72080;

/**
 * @brief Cursor Base Address Register (CUR_BASE).
 *
 * Specifies bits [31:12] of the GTT-relative graphics address of the cursor
 * surface. Writing this register **arms** all double-buffered cursor
 * registers for this pipe.
 *
 * @note The cursor surface must be in **linear memory** (cannot be tiled)
 *       and must be at least **4 KB aligned**.
 *
 * @note **VT-d Workaround:** To prevent false VT-d type 6 errors, align the
 *       cursor surface address to **64 KB** and allocate **2 extra PTEs**
 *       beyond the end of the displayed surface (only the PTEs are used,
 *       not the backing pages).
 *
 * @note When performing 180-degree rotation, this address does not need to
 *       change; hardware internally offsets to start from the last pixel
 *       of the last line.
 *
 * @note Double-buffered. Update takes effect at the start of the next
 *       vertical blank, or immediately if the pipe is not enabled.
 *
 * <b>Register addresses:</b>
 * | Pipe | Address | Power Well |
 * |------|---------|------------|
 * | A    | 0x70084 | PG1        |
 * | B    | 0x71084 | PG2        |
 * | C    | 0x72084 | PG2        |
 *
 * @see IHD-OS-KBL-Vol 2c-1.17, pp. 384–385 (CUR_BASE)
 */
union CUR_BASE {
    struct {
        u32 reserved0 : 12; ///< [11:0]  Reserved
        u32 base_addr : 20; ///< [31:12] Cursor base graphics address bits [31:12]
    } __attribute__((packed));

    u32 raw;

    /**
     * @brief Returns the full 32-bit GTT byte address of the cursor surface.
     */
    [[nodiscard]] constexpr u32 address() const {
        return base_addr << 12;
    }

    /**
     * @brief Sets the cursor surface base address.
     * @param gtt_addr  GTT-relative byte address (min 4 KB aligned; 64 KB for VT-d WA).
     */
    constexpr void set_address(u32 gtt_addr) {
        base_addr = gtt_addr >> 12;
    }
};

static_assert(sizeof(CUR_BASE) == 4, "CUR_BASE must be 32 bits");

constexpr u32 CUR_BASE_A = 0x70084;
constexpr u32 CUR_BASE_B = 0x71084;
constexpr u32 CUR_BASE_C = 0x72084;

/**
 * @brief Cursor Position Register (CUR_POS).
 *
 * Specifies the screen position of the cursor's upper-left corner relative
 * to the upper-left corner of the display pipe source image area.
 *
 * Coordinates use **sign-magnitude** encoding (not two's complement), allowing
 * negative coordinates when the cursor partially extends past the top or left
 * edge of the pipe source area.
 *
 * @note At least a single pixel of the cursor must be positioned over the
 *       pipe source area.
 *
 * @note When 180-degree rotation is enabled, hardware rotates the image but
 *       not the position; software must adjust coordinates accordingly.
 *
 * @note Double-buffered. Update takes effect at the start of the next
 *       vertical blank, or immediately if the pipe is not enabled.
 *
 * <b>Register addresses:</b>
 * | Pipe | Address | Power Well |
 * |------|---------|------------|
 * | A    | 0x70088 | PG1        |
 * | B    | 0x71088 | PG2        |
 * | C    | 0x72088 | PG2        |
 *
 * @see IHD-OS-KBL-Vol 2c-1.17, pp. 394–395 (CUR_POS)
 */
union CUR_POS {
    struct {
        u32 x_mag : 13;      ///< [12:0]  X Position Magnitude in pixels
        u32 reserved13 : 2;  ///< [14:13] MBZ
        u32 x_sign : 1;      ///< [15]    X Position Sign (0=positive, 1=negative)
        u32 y_mag : 12;      ///< [27:16] Y Position Magnitude in lines
        u32 reserved28 : 3;  ///< [30:28] MBZ
        u32 y_sign : 1;      ///< [31]    Y Position Sign (0=positive, 1=negative)
    } __attribute__((packed));

    u32 raw;

    /**
     * @brief Returns the signed horizontal cursor position in pixels.
     */
    [[nodiscard]] constexpr i32 x() const {
        return x_sign ? -static_cast<i32>(x_mag) : static_cast<i32>(x_mag);
    }

    /**
     * @brief Returns the signed vertical cursor position in lines.
     */
    [[nodiscard]] constexpr i32 y() const {
        return y_sign ? -static_cast<i32>(y_mag) : static_cast<i32>(y_mag);
    }

    /**
     * @brief Sets the cursor position from signed coordinates (sign-magnitude encoding).
     * @param px  Horizontal position in pixels ([-8191, 8191]).
     * @param py  Vertical position in lines ([-4095, 4095]).
     */
    constexpr void set(i32 px, i32 py) {
        if (px < 0) {
            x_sign = 1;
            x_mag = static_cast<u32>(-px) & 0x1FFFu;
        } else {
            x_sign = 0;
            x_mag = static_cast<u32>(px) & 0x1FFFu;
        }

        if (py < 0) {
            y_sign = 1;
            y_mag = static_cast<u32>(-py) & 0x0FFFu;
        } else {
            y_sign = 0;
            y_mag = static_cast<u32>(py) & 0x0FFFu;
        }
    }
};

static_assert(sizeof(CUR_POS) == 4, "CUR_POS must be 32 bits");

constexpr u32 CUR_POS_A = 0x70088;
constexpr u32 CUR_POS_B = 0x71088;
constexpr u32 CUR_POS_C = 0x72088;

/**
 * @brief Cursor Palette Register (CUR_PAL).
 *
 * Provides RGB color entries for 2 bpp indexed cursor modes. There are 4
 * palette registers per pipe (indices 0–3, spaced 4 bytes apart).
 *
 * <b>Index mapping by cursor mode:</b>
 * | Index | 2-color (AND/XOR)  | 3-color     | 4-color   |
 * |-------|--------------------|-------------|-----------|
 * | 00b   | CUR_PAL_0          | CUR_PAL_0   | CUR_PAL_0 |
 * | 01b   | CUR_PAL_1          | CUR_PAL_1   | CUR_PAL_1 |
 * | 10b   | Transparent        | Transparent | CUR_PAL_2 |
 * | 11b   | Invert Destination | CUR_PAL_3   | CUR_PAL_3 |
 *
 * @note Double-buffered. Update takes effect at the start of the next
 *       vertical blank, or immediately if the pipe is not enabled.
 *
 * <b>Register addresses:</b>
 * | Pipe | Index 0 | Index 1 | Index 2 | Index 3 | Power Well |
 * |------|---------|---------|---------|---------|------------|
 * | A    | 0x70090 | 0x70094 | 0x70098 | 0x7009C | PG1        |
 * | B    | 0x71090 | 0x71094 | 0x71098 | 0x7109C | PG2        |
 * | C    | 0x72090 | 0x72094 | 0x72098 | 0x7209C | PG2        |
 *
 * @see IHD-OS-KBL-Vol 2c-1.17, pp. 392–393 (CUR_PAL)
 */
union CUR_PAL {
    struct {
        u32 blue : 8;       ///< [7:0]   Palette Blue value
        u32 green : 8;      ///< [15:8]  Palette Green value
        u32 red : 8;        ///< [23:16] Palette Red value
        u32 reserved24 : 8; ///< [31:24] Reserved
    } __attribute__((packed));

    u32 raw;

    constexpr void set_rgb(u8 r, u8 g, u8 b) {
        red = r;
        green = g;
        blue = b;
        reserved24 = 0;
    }
};

static_assert(sizeof(CUR_PAL) == 4, "CUR_PAL must be 32 bits");

constexpr u32 CUR_PAL_A_0 = 0x70090;
constexpr u32 CUR_PAL_A_1 = 0x70094;
constexpr u32 CUR_PAL_A_2 = 0x70098;
constexpr u32 CUR_PAL_A_3 = 0x7009C;

constexpr u32 CUR_PAL_B_0 = 0x71090;
constexpr u32 CUR_PAL_B_1 = 0x71094;
constexpr u32 CUR_PAL_B_2 = 0x71098;
constexpr u32 CUR_PAL_B_3 = 0x7109C;

constexpr u32 CUR_PAL_C_0 = 0x72090;
constexpr u32 CUR_PAL_C_1 = 0x72094;
constexpr u32 CUR_PAL_C_2 = 0x72098;
constexpr u32 CUR_PAL_C_3 = 0x7209C;

/**
 * @brief Cursor FBC Control Register (CUR_FBC_CTL).
 *
 * Controls cursor vertical size reduction, allowing the cursor engine to
 * fetch and display fewer scan lines and output transparency for the
 * remainder of the cursor height.
 *
 * @note Not allowed with 2 bpp cursor formats or 180-degree cursor rotation.
 *
 * @note @c reduced_scan_lines stores the line count minus one. Minimum valid
 *       size is 8 lines (programmed as @c 0x07); maximum cannot exceed the
 *       normal height of the selected cursor mode.
 *
 * @note Double-buffered. Armed by a write to @c CUR_BASE (or when the cursor
 *       is not enabled). Update takes effect at the start of the next
 *       vertical blank, or immediately if the pipe is not enabled.
 *
 * <b>Register addresses:</b>
 * | Pipe | Address | Power Well |
 * |------|---------|------------|
 * | A    | 0x700A0 | PG1        |
 * | B    | 0x710A0 | PG2        |
 * | C    | 0x720A0 | PG2        |
 *
 * @see IHD-OS-KBL-Vol 2c-1.17, pp. 390–391 (CUR_FBC_CTL)
 */
union CUR_FBC_CTL {
    struct {
        u32 reduced_scan_lines : 8;    ///< [7:0]   Number of scan lines to fetch, minus one (min 7 -> 8 lines)
        u32 reserved8 : 23;            ///< [30:8]  Reserved
        u32 size_reduction_enable : 1; ///< [31]    Size Reduction Enable (0=Disable, 1=Enable)
    } __attribute__((packed));

    u32 raw;

    /**
     * @brief Returns the actual reduced scan line count.
     */
    [[nodiscard]] constexpr u32 scan_lines() const {
        return reduced_scan_lines + 1;
    }

    /**
     * @brief Sets the reduced scan line count.
     * @param lines  Actual number of lines (min 8, max <= normal cursor height).
     */
    constexpr void set_scan_lines(u32 lines) {
        reduced_scan_lines = (lines - 1) & 0xFFu;
    }
};

static_assert(sizeof(CUR_FBC_CTL) == 4, "CUR_FBC_CTL must be 32 bits");

constexpr u32 CUR_FBC_CTL_A = 0x700A0;
constexpr u32 CUR_FBC_CTL_B = 0x710A0;
constexpr u32 CUR_FBC_CTL_C = 0x720A0;

/**
 * @brief Cursor Live Base Address Register (CUR_SURFLIVE).
 *
 * Read-only register reporting bits [31:12] of the surface base address
 * currently in active use by the cursor hardware.
 *
 * <b>Register addresses:</b>
 * | Pipe | Address | Power Well |
 * |------|---------|------------|
 * | A    | 0x700AC | PG1        |
 * | B    | 0x710AC | PG2        |
 * | C    | 0x720AC | PG2        |
 *
 * @see IHD-OS-KBL-Vol 2c-1.17, p. 396 (CUR_SURFLIVE)
 */
union CUR_SURFLIVE {
    struct {
        u32 reserved0 : 12;         ///< [11:0]  MBZ
        u32 live_surface_addr : 20; ///< [31:12] Live surface base address bits [31:12] (RO)
    } __attribute__((packed));

    u32 raw;

    /**
     * @brief Returns the live 32-bit GTT byte address currently scanned out.
     */
    [[nodiscard]] constexpr u32 address() const {
        return live_surface_addr << 12;
    }
};

static_assert(sizeof(CUR_SURFLIVE) == 4, "CUR_SURFLIVE must be 32 bits");

constexpr u32 CUR_SURFLIVE_A = 0x700AC;
constexpr u32 CUR_SURFLIVE_B = 0x710AC;
constexpr u32 CUR_SURFLIVE_C = 0x720AC;

#endif //VESPERAOS_CURSOR_REGS_H
