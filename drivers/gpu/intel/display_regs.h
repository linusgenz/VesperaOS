// display_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 25.05.26.
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
#ifndef VESPERAOS_DISPLAY_REGS_H
#define VESPERAOS_DISPLAY_REGS_H

/**
 * @brief Plane Control Register (PLANE_CTL).
 *
 * Controls all aspects of a single display plane: pixel format, tiling,
 * color space conversion, alpha blending, color keying, rotation, and
 * gamma correction.
 *
 * @note Double-buffered. Shadow registers are armed by a write to
 *       @c PLANE_SURF (or when the plane is not enabled). The update
 *       takes effect at the start of the next vertical blank, or
 *       immediately if the pipe is not enabled.
 *
 * @note When plane scaling is enabled, plane size and scaler registers
 *       must be programmed early in the active region. Programming them
 *       close to VBlank may cause partial incorrect programming and
 *       screen corruption.
 *
 * <b>Register addresses:</b>
 * | Pipe | Plane 1  | Plane 2  | Plane 3  |
 * |------|----------|----------|----------|
 * | A    | 0x70180  | 0x70280  | 0x70380  |
 * | B    | 0x71180  | 0x71280  | 0x71380  |
 * | C    | 0x72180  | 0x72280  | 0x72380  |
 *
 * @see IHD-OS-KBL-Vol 2c-1.17, pp. 562–569 (PLANE_CTL)
 */
union PLANE_CTL {
    enum Rotation : u32 {
        ROTATE_0 = 0b00,
        ROTATE_90 = 0b01,  ///< Requires Y-Tiled surface
        ROTATE_180 = 0b10,
        ROTATE_270 = 0b11,  ///< Requires Y-Tiled surface
    };

    enum AlphaMode : u32 {
        ALPHA_DISABLE = 0b00,    ///< Alpha channel ignored
        ALPHA_SW_PREMUL = 0b10,  ///< Pre-multiplied by software
        ALPHA_HW_PREMUL = 0b11,  ///< Pre-multiplied by hardware
        // Note: only supported with RGB8888 formats; incompatible with FBC
    };

    enum StereoVblankMask : u32 {
        STEREO_BOTH_EYES = 0b00,
        STEREO_MASK_LEFT = 0b01,
        STEREO_MASK_RIGHT = 0b10,
    };

    enum TiledSurface : u32 {
        TILING_LINEAR = 0b000,
        TILING_X = 0b001,
        TILING_Y = 0b100,   ///< Tile Y (Legacy)
        TILING_YF = 0b101,  ///< Tile Y F
    };

    enum Yuv422ByteOrder : u32 {
        YUV422_YUYV = 0b00,
        YUV422_UYVY = 0b01,
        YUV422_YVYU = 0b10,
        YUV422_VYUY = 0b11,
    };

    enum KeyEnable : u32 {
        KEY_DISABLE = 0b00,
        KEY_SOURCE = 0b01,         ///< Matched pixels treated as transparent
        KEY_DESTINATION = 0b10,    ///< Matched pixels make plane above opaque
        KEY_SOURCE_WINDOW = 0b11,  ///< Transparent where plane below is opaque
    };

    enum PixelFormat : u32 {
        FMT_YUV422_16BIT = 0b0000,      ///< YUV 16-bit 4:2:2 packed
        FMT_NV12 = 0b0001,              ///< YUV 4:2:0 planar — plane 1/2 of pipe A/B only
        FMT_RGB_2_10_10_10 = 0b0010,    ///< RGB 32-bit 2:10:10:10
        FMT_RGB_8_8_8_8 = 0b0100,       ///< RGB 32-bit 8:8:8:8
        FMT_RGB_16_16_16_16F = 0b0110,  ///< RGB 64-bit 16:16:16:16 float — no scaling
        FMT_YUV444_8_8_8_8 = 0b1000,    ///< YUV 32-bit 4:4:4 packed
        FMT_RGB_XR_BIAS = 0b1010,       ///< RGB 32-bit XR_BIAS 2:10:10:10 — no scaling
        FMT_INDEXED_8BIT = 0b1100,      ///< 8-bit indexed — no scaling, no color keying
        FMT_RGB_5_6_5 = 0b1110,         ///< RGB 16-bit 5:6:5
    };

    struct {
        Rotation plane_rotation : 2;              ///< [1:0]   Plane Rotation (see Rotation enum)
        u32 reserved2 : 1;                        ///< [2]     MBZ
        u32 allow_db_update_disable : 1;          ///< [3]     Allow Double Buffer Update Disable
        AlphaMode alpha_mode : 2;                 ///< [5:4]   Alpha Mode (see AlphaMode enum)
        StereoVblankMask stereo_vblank_mask : 2;  ///< [7:6]   Stereo Surface Vblank Mask (see StereoVblankMask enum)
        u32 reserved8 : 1;                        ///< [8]     MBZ
        u32 async_addr_update : 1;                ///< [9]     Async Address Update Enable
        TiledSurface tiled_surface : 3;           ///< [12:10] Tiled Surface (see TiledSurface enum)
        u32 plane_gamma_disable : 1;              ///< [13]    Plane Gamma Disable (0=enabled, 1=disabled)
        u32 trickle_feed_enable : 1;              ///< [14]    Trickle Feed Enable — do not set to 1
        u32 render_decomp : 1;                    ///< [15]    Render Decompression Enable
        Yuv422ByteOrder yuv422_byte_order : 2;    ///< [17:16] YUV 4:2:2 Byte Order (see Yuv422ByteOrder enum)
        u32 yuv_csc_format : 1;                   ///< [18]    Plane YUV-to-RGB CSC Format (0=BT.601, 1=BT.709)
        u32 yuv_csc_disable : 1;                  ///< [19]    Plane YUV-to-RGB CSC Disable (0=enabled, 1=disabled)
        u32 rgb_color_order : 1;                  ///< [20]    RGB Color Order (0=BGRX, 1=RGBX)
        KeyEnable key_enable : 2;                 ///< [22:21] Color Key Enable (see KeyEnable enum)
        u32 pipe_csc_enable : 1;                  ///< [23]    Pipe CSC Enable
        PixelFormat source_pixel_format : 4;      ///< [27:24] Source Pixel Format (see PixelFormat enum)
        u32 yuv_range_corr_disable : 1;           ///< [28]    YUV Range Correction Disable
        u32 remove_yuv_offset : 1;                ///< [29]    Remove YUV Offset (0=remove, 1=preserve)
        u32 pipe_gamma_enable : 1;                ///< [30]    Pipe Gamma Enable
        u32 plane_enable : 1;                     ///< [31]    Plane Enable
    } __attribute__((packed));

    u32 raw;
};

static_assert(sizeof(PLANE_CTL) == 4, "PLANE_CTL must be 32 bits");

constexpr u32 PLANE_CTL_1_A = 0x70180;
constexpr u32 PLANE_CTL_2_A = 0x70280;
constexpr u32 PLANE_CTL_3_A = 0x70380;
constexpr u32 PLANE_CTL_1_B = 0x71180;
constexpr u32 PLANE_CTL_2_B = 0x71280;
constexpr u32 PLANE_CTL_3_B = 0x71380;
constexpr u32 PLANE_CTL_1_C = 0x72180;
constexpr u32 PLANE_CTL_2_C = 0x72280;
constexpr u32 PLANE_CTL_3_C = 0x72380;

/**
 * @brief Plane Stride Register (PLANE_STRIDE).
 *
 * Specifies the line-to-line increment (stride) for a display plane surface.
 * The unit of the 10-bit @c stride field depends on the tiling format of the
 * surface programmed in @c PLANE_CTL:
 *
 * | Tiling Format    | Stride Unit         | Tile Width (bytes) |
 * |------------------|---------------------|--------------------|
 * | Linear           | 64 bytes (1 cacheline) | —               |
 * | Tile X           | 1 tile              | 512                |
 * | Tile Y (Legacy)  | 1 tile              | 128                |
 * | Tile YF (8 bpp)  | 1 tile              | 64                 |
 * | Tile YF (16/32/64 bpp) | 1 tile        | 128                |
 *
 * @note Double-buffered. Shadow registers are armed by a write to
 *       @c PLANE_SURF (or when the plane is not enabled). The update
 *       takes effect at the start of the next vertical blank, or
 *       immediately if the pipe is not enabled.
 *
 * @note For YUV planar formats (NV12, P0xx), the UV surface stride must
 *       be programmed separately in @c PLANE_AUX_DIST. The stride in bytes
 *       must be equal for the Y and UV surfaces.
 *
 * @note In Tile YF format with a YUV planar Y surface (non-rotated), the
 *       programmed stride value must be an even number of tiles.
 *
 * @note The stride in bytes must not exceed the lesser of 8K pixels or
 *       32K bytes. See the maximum stride table in the spec for per-format
 *       tile limits.
 *
 * <b>Register addresses:</b>
 * | Pipe | Plane 1  | Plane 2  | Plane 3  |
 * |------|----------|----------|----------|
 * | A    | 0x70188  | 0x70288  | 0x70388  |
 * | B    | 0x71188  | 0x71288  | 0x71388  |
 * | C    | 0x72188  | 0x72288  | 0x72388  |
 *
 * @see IHD-OS-KBL-Vol 2c-1.17, pp. 604–606 (PLANE_STRIDE)
 * @see PLANE_AUX_DIST for the auxiliary (UV) surface stride in NV12/P0xx mode.
 */
union PLANE_STRIDE {
    struct {
        u32 stride : 10;    ///< [9:0]   Stride in units depending on tiling format
        u32 reserved : 22;  ///< [31:10] MBZ
    } __attribute__((packed));

    u32 raw;

    /**
     * @brief Returns the stride in bytes for the given tiling format.
     * @param tiling  The tiling mode from @c PLANE_CTL::TiledSurface.
     * @param bpp     Bytes per pixel (used for Tile YF only: 1, 2, 4, or 8).
     */
    [[nodiscard]] constexpr u32 stride_bytes(PLANE_CTL::TiledSurface tiling, u32 bpp = 4) const {
        switch (tiling) {
            case PLANE_CTL::TILING_LINEAR:
                return stride * 64;
            case PLANE_CTL::TILING_X:
                return stride * 512;
            case PLANE_CTL::TILING_Y:
                return stride * 128;
            case PLANE_CTL::TILING_YF:
                return stride * (bpp == 1 ? 64 : 128);
            default:
                return stride * 64;
        }
    }

    /**
     * @brief Sets the stride field from a byte stride value.
     * @param bytes   Desired stride in bytes.
     * @param tiling  The tiling mode from @c PLANE_CTL::TiledSurface.
     * @param bpp     Bytes per pixel (used for Tile YF only).
     */
    constexpr void set_stride_bytes(u32 bytes, PLANE_CTL::TiledSurface tiling, u32 bpp = 4) {
        switch (tiling) {
            case PLANE_CTL::TILING_LINEAR:
                stride = bytes / 64;
                break;
            case PLANE_CTL::TILING_X:
                stride = bytes / 512;
                break;
            case PLANE_CTL::TILING_Y:
                stride = bytes / 128;
                break;
            case PLANE_CTL::TILING_YF:
                stride = bytes / (bpp == 1 ? 64 : 128);
                break;
            default:
                stride = bytes / 64;
                break;
        }
    }
};

static_assert(sizeof(PLANE_STRIDE) == 4, "PLANE_STRIDE must be 32 bits");

constexpr u32 PLANE_STRIDE_1_A = 0x70188;
constexpr u32 PLANE_STRIDE_2_A = 0x70288;
constexpr u32 PLANE_STRIDE_3_A = 0x70388;
constexpr u32 PLANE_STRIDE_1_B = 0x71188;
constexpr u32 PLANE_STRIDE_2_B = 0x71288;
constexpr u32 PLANE_STRIDE_3_B = 0x71388;
constexpr u32 PLANE_STRIDE_1_C = 0x72188;
constexpr u32 PLANE_STRIDE_2_C = 0x72288;
constexpr u32 PLANE_STRIDE_3_C = 0x72388;

/**
 * @brief Plane Size Register (PLANE_SIZE).
 *
 * Specifies the source size of the plane — the region fetched from the
 * framebuffer. Both @c width and @c height are stored as value minus one,
 * so a 1920×1080 plane is programmed as @c width=1919, @c height=1079.
 *
 * @note When plane scaling is **disabled**, this is also the size of the
 *       plane as blended with other planes on the pipe. The plane must be
 *       fully contained within the pipe source area:
 *       @code plane_position + plane_size <= pipe_source_size @endcode
 *
 * @note When plane scaling is **enabled**, the scaler window size
 *       determines the blended size. The height must be at least 8 lines.
 *
 * @note The @c width must be even (i.e. the programmed value must be odd)
 *       when the source pixel format is YUV 4:2:2 or YUV 4:2:0 (NV12).
 *       The width must not exceed the stride in pixels.
 *
 * <b>Maximum width per tiling format:</b>
 * | Tiling              | Bytes/pixel | Max width (pixels, no H-pan) |
 * |---------------------|-------------|------------------------------|
 * | Linear, Tile X      | 8           | 4096                         |
 * | Linear, Tile X      | 1, 2, 4     | 8192                         |
 * | Tile Yb / Tile YF   | 8           | 2048                         |
 * | Tile Yb / Tile YF   | 4           | 4096                         |
 * | Tile Yb / Tile YF   | 1, 2        | 8192                         |
 *
 * @note Double-buffered. Shadow registers are armed by a write to
 *       @c PLANE_SURF (or when the plane is not enabled). The update
 *       takes effect at the start of the next vertical blank, or
 *       immediately if the pipe is not enabled.
 *
 * <b>Register addresses:</b>
 * | Pipe | Plane 1  | Plane 2  | Plane 3  |
 * |------|----------|----------|----------|
 * | A    | 0x70190  | 0x70290  | 0x70390  |
 * | B    | 0x71190  | 0x71290  | 0x71390  |
 * | C    | 0x72190  | 0x72290  | 0x72390  |
 *
 * @see IHD-OS-KBL-Vol 2c-1.17, pp. 600–602 (PLANE_SIZE)
 * @see PLANE_POS  for the plane's position within the pipe source area.
 * @see PLANE_CTL  for the source pixel format and tiling mode.
 */
union PLANE_SIZE {
    struct {
        u32 width : 13;      ///< [12:0]  Plane width  in pixels, minus one (max 8191 → 8192 px)
        u32 reserved13 : 3;  ///< [15:13] MBZ
        u32 height : 12;     ///< [27:16] Plane height in lines,  minus one (max 4095 → 4096 lines)
        u32 reserved28 : 4;  ///< [31:28] MBZ
    } __attribute__((packed));

    u32 raw;

    /**
     * @brief Returns the actual plane width in pixels.
     */
    [[nodiscard]] constexpr u32 width_px() const {
        return width + 1;
    }

    /**
     * @brief Returns the actual plane height in lines.
     */
    [[nodiscard]] constexpr u32 height_px() const {
        return height + 1;
    }

    /**
     * @brief Sets width and height from actual pixel dimensions.
     * @param w  Actual width  in pixels (1–8192).
     * @param h  Actual height in lines  (1–4096).
     */
    constexpr void set(u32 w, u32 h) {
        width = w - 1;
        height = h - 1;
    }
};

static_assert(sizeof(PLANE_SIZE) == 4, "PLANE_SIZE must be 32 bits");

constexpr u32 PLANE_SIZE_1_A = 0x70190;
constexpr u32 PLANE_SIZE_2_A = 0x70290;
constexpr u32 PLANE_SIZE_3_A = 0x70390;
constexpr u32 PLANE_SIZE_1_B = 0x71190;
constexpr u32 PLANE_SIZE_2_B = 0x71290;
constexpr u32 PLANE_SIZE_3_B = 0x71390;
constexpr u32 PLANE_SIZE_1_C = 0x72190;
constexpr u32 PLANE_SIZE_2_C = 0x72290;
constexpr u32 PLANE_SIZE_3_C = 0x72390;

/**
 * @brief Plane Position Register (PLANE_POS).
 *
 * Specifies the screen position of the plane's upper-left corner, relative
 * to the upper-left corner of the pipe source image area.
 *
 * @note When plane scaling is **disabled**, this is the position of the plane
 *       as blended with other planes on the pipe. The plane must be fully
 *       contained within the pipe source area:
 *       @code plane_position + plane_size <= pipe_source_size @endcode
 *
 * @note When plane scaling is **enabled**, the scaler window position
 *       determines the blended position. In this case both @c x and @c y
 *       must be programmed to zero.
 *
 * @note Hardware rotates the plane image but does not adjust the position.
 *       When rotation is enabled, software must update this register manually
 *       to maintain the correct apparent position on a physically rotated
 *       display.
 *
 * @note Double-buffered. Shadow registers are armed by a write to
 *       @c PLANE_SURF (or when the plane is not enabled). The update
 *       takes effect at the start of the next vertical blank, or
 *       immediately if the pipe is not enabled.
 *
 * <b>Register addresses:</b>
 * | Pipe | Plane 1  | Plane 2  | Plane 3  |
 * |------|----------|----------|----------|
 * | A    | 0x7018C  | 0x7028C  | 0x7038C  |
 * | B    | 0x7118C  | 0x7128C  | 0x7138C  |
 * | C    | 0x7218C  | 0x7228C  | 0x7238C  |
 *
 * @see IHD-OS-KBL-Vol 2c-1.17, pp. 598–600 (PLANE_POS)
 * @see PLANE_SIZE for the plane dimensions used in the containment check.
 * @see PLANE_CTL  for rotation mode.
 */
union PLANE_POS {
    struct {
        u32 x : 13;          ///< [12:0]  Horizontal position of upper-left corner in pixels
        u32 reserved13 : 3;  ///< [15:13] MBZ
        u32 y : 12;          ///< [27:16] Vertical position of upper-left corner in lines
        u32 reserved28 : 4;  ///< [31:28] MBZ
    } __attribute__((packed));

    u32 raw;

    /**
     * @brief Sets the plane position.
     * @param px  Horizontal position in pixels.
     * @param py  Vertical position in lines.
     * @note When plane scaling is enabled, both @p px and @p py must be zero.
     */
    constexpr void set(u32 px, u32 py) {
        x = px;
        y = py;
    }
};

static_assert(sizeof(PLANE_POS) == 4, "PLANE_POS must be 32 bits");

constexpr u32 PLANE_POS_1_A = 0x7018C;
constexpr u32 PLANE_POS_2_A = 0x7028C;
constexpr u32 PLANE_POS_3_A = 0x7038C;
constexpr u32 PLANE_POS_1_B = 0x7118C;
constexpr u32 PLANE_POS_2_B = 0x7128C;
constexpr u32 PLANE_POS_3_B = 0x7138C;
constexpr u32 PLANE_POS_1_C = 0x7218C;
constexpr u32 PLANE_POS_2_C = 0x7228C;
constexpr u32 PLANE_POS_3_C = 0x7238C;

/**
 * @brief Plane Offset / Plane Auxiliary Offset Register (PLANE_OFFSET / PLANE_AUX_OFFSET).
 *
 * Specifies the panning offset for the plane surface. The @c x and @c y fields
 * define the upper-left starting position within the surface buffer, allowing
 * a sub-region of a larger surface to be displayed (hardware panning).
 *
 * @note @c PLANE_OFFSET applies to the primary (Y) surface.
 *       @c PLANE_AUX_OFFSET applies to the auxiliary (UV) surface in YUV planar
 *       formats (NV12, P0xx) and must be programmed separately at its own address.
 *
 * <b>Rotation behaviour:</b>
 * - **0°**:   Offsets applied directly; X must be even-pixel-aligned for YUV 4:2:2
 *             and YUV 4:2:0 formats.
 * - **90°**:  Software must account for rotation:
 *             @code
 *             x_offset = (surface_height_in_tiles * tile_height) - y_offset - y_size
 *             y_offset = original_x_offset
 *             @endcode
 *             Y offset must be even-line-aligned for YUV 4:2:2 and YUV 4:2:0 formats.
 * - **180°**: Hardware internally adds the plane size to both offsets, so display
 *             starts from the bottom-right corner. No manual adjustment needed.
 * - **270°**: Same programming as 90° rotation.
 *
 * <b>YUV planar (NV12) auxiliary surface offsets:</b>
 * - If the UV surface is tile-row-aligned:
 *   @code uv_offset = y_offset / 2 @endcode
 * - If the UV surface is **not** tile-row-aligned, the UV Y offset must additionally
 *   include the lines from the previous nearest tile-row-aligned address.
 *
 * @note The plane size plus offset must not exceed the maximum supported plane size.
 *
 * @note Double-buffered. Update takes effect at the start of the next vertical
 *       blank, or immediately if the pipe or plane is not enabled.
 *
 * <b>Register addresses (PLANE_OFFSET / PLANE_AUX_OFFSET):</b>
 * | Pipe | Plane 1             | Plane 2             | Plane 3             |
 * |------|---------------------|---------------------|---------------------|
 * | A    | 0x701A4 / 0x701C4  | 0x702A4 / 0x702C4  | 0x703A4 / 0x703C4  |
 * | B    | 0x711A4 / 0x711C4  | 0x712A4 / 0x712C4  | 0x713A4 / 0x713C4  |
 * | C    | 0x721A4 / 0x721C4  | 0x722A4 / 0x722C4  | 0x723A4 / 0x723C4  |
 *
 * @see IHD-OS-KBL-Vol 2c-1.17, pp. 594–597 (PLANE_OFFSET)
 * @see PLANE_AUX_DIST for the auxiliary surface base address offset in NV12 mode.
 * @see PLANE_SIZE     for the plane dimensions used in the bounds check.
 * @see PLANE_CTL      for rotation mode and source pixel format.
 */
union PLANE_OFFSET {
    struct {
        u32 x : 13;          ///< [12:0]  Start X position — horizontal offset in pixels from surface origin
        u32 reserved13 : 3;  ///< [15:13] MBZ
        u32 y : 13;          ///< [28:16] Start Y position — vertical offset in lines from surface origin
        u32 reserved29 : 3;  ///< [31:29] MBZ
    } __attribute__((packed));

    u32 raw;

    /**
     * @brief Sets the panning offset.
     * @param px  Horizontal offset in pixels from the surface origin.
     * @param py  Vertical offset in lines from the surface origin.
     * @note For YUV 4:2:2 / 4:2:0 in 0°/180° mode, @p px must be even.
     *       For YUV 4:2:2 / 4:2:0 in 90°/270° mode, @p py must be even.
     */
    constexpr void set(u32 px, u32 py) {
        x = px;
        y = py;
    }
};

static_assert(sizeof(PLANE_OFFSET) == 4, "PLANE_OFFSET must be 32 bits");

constexpr u32 PLANE_OFFSET_1_A = 0x701A4;
constexpr u32 PLANE_OFFSET_2_A = 0x702A4;
constexpr u32 PLANE_OFFSET_3_A = 0x703A4;
constexpr u32 PLANE_OFFSET_1_B = 0x711A4;
constexpr u32 PLANE_OFFSET_2_B = 0x712A4;
constexpr u32 PLANE_OFFSET_3_B = 0x713A4;
constexpr u32 PLANE_OFFSET_1_C = 0x721A4;
constexpr u32 PLANE_OFFSET_2_C = 0x722A4;
constexpr u32 PLANE_OFFSET_3_C = 0x723A4;

constexpr u32 PLANE_AUX_OFFSET_1_A = 0x701C4;
constexpr u32 PLANE_AUX_OFFSET_2_A = 0x702C4;
constexpr u32 PLANE_AUX_OFFSET_3_A = 0x703C4;
constexpr u32 PLANE_AUX_OFFSET_1_B = 0x711C4;
constexpr u32 PLANE_AUX_OFFSET_2_B = 0x712C4;
constexpr u32 PLANE_AUX_OFFSET_3_B = 0x713C4;
constexpr u32 PLANE_AUX_OFFSET_1_C = 0x721C4;
constexpr u32 PLANE_AUX_OFFSET_2_C = 0x722C4;
constexpr u32 PLANE_AUX_OFFSET_3_C = 0x723C4;

/**
 * @brief Plane Surface Base Address Register (PLANE_SURF).
 *
 * Specifies the GTT-relative base address of the plane's framebuffer surface.
 * Writing this register **arms** all double-buffered primary registers for the
 * plane and is treated as a flip — it can trigger a flip-done interrupt if the
 * corresponding interrupt registers are configured accordingly.
 *
 * @note The address field holds bits [31:12] of a GTT graphics address
 *       (4 KB page granularity). The address is an offset from the graphics
 *       memory aperture base and is mapped to physical pages through the
 *       global GTT (GGTT).
 *
 * <b>Address alignment requirements:</b>
 * | Tiling Mode      | Minimum alignment |
 * |------------------|-------------------|
 * | Linear, Tile X   | 256 KB            |
 * | Tile Y (any)     | 1 MB              |
 *
 * <b>PTE padding requirements:</b>
 * - Allocate 136 extra PTEs **beyond** the end of the displayed surface.
 * - If 180° or 270° rotation is required, also allocate 136 extra PTEs
 *   **before** the beginning of the surface. Wrap around when address range
 *   limits are reached. Only the PTEs are consumed, not the backing pages.
 * - For render compression: padding must be added for both the main surface
 *   and the compression control surface, with padding between them.
 * - For planar YUV 4:2:0 (NV12): padding must be added for both Y and UV
 *   surfaces, with padding between them.
 *
 * <b>Flip timing:</b>
 * - **Synchronous** (MMIO or CS flip, non-stereo): update at start of next
 *   vertical blank.
 * - **Asynchronous** (async MMIO or CS flip): update at next TLB request or
 *   start of next vertical blank, whichever comes first. Completion can take
 *   up to one full frame; disable FBC and render compression and keep the
 *   data buffer small for faster completion.
 * - **Stereo 3D synchronous**: update at the start of the left or right eye
 *   vertical blank, selected by @c PLANE_CTL::stereo_vblank_mask.
 *   In stereo 3D mode, @c surface_addr holds the **right-eye** base address.
 *
 * @note Double-buffered. Update takes effect at the start of the next vertical
 *       blank (or immediately if the pipe or plane is not enabled).
 *
 * <b>Register addresses:</b>
 * | Pipe | Plane 1  | Plane 2  | Plane 3  |
 * |------|----------|----------|----------|
 * | A    | 0x7019C  | 0x7029C  | 0x7039C  |
 * | B    | 0x7119C  | 0x7129C  | 0x7139C  |
 * | C    | 0x7219C  | 0x7229C  | 0x7239C  |
 *
 * @see IHD-OS-KBL-Vol 2c-1.17, pp. 608–610 (PLANE_SURF)
 * @see PLANE_CTL       for tiling mode, rotation, and stereo vblank mask.
 * @see PLANE_AUX_DIST  for the auxiliary (UV) surface address in NV12 mode.
 */
union PLANE_SURF {
    struct {
        u32 reserved0 : 3;      ///< [2:0]   MBZ
        u32 ring_flip_src : 1;  ///< [3]     Ring Flip Source (RO): 0=CS, 1=BCS
        u32 reserved4 : 8;      ///< [11:4]  MBZ
        u32 surface_addr : 20;  ///< [31:12] Surface base address bits [31:12], GTT-relative
    } __attribute__((packed));

    u32 raw;

    /**
     * @brief Returns the full 32-bit GTT byte address of the surface.
     * @note Bits [11:0] are implicitly zero (4 KB page granularity).
     */
    [[nodiscard]] constexpr u32 address() const {
        return surface_addr << 12;
    }

    /**
     * @brief Sets the surface base address.
     * @param gtt_addr  GTT-relative byte address. Must be 4 KB aligned.
     *                  Linear/Tile X: must be 256 KB aligned.
     *                  Tile Y: must be 1 MB aligned.
     */
    constexpr void set_address(u32 gtt_addr) {
        surface_addr = gtt_addr >> 12;
    }
};

static_assert(sizeof(PLANE_SURF) == 4, "PLANE_SURF must be 32 bits");

constexpr u32 PLANE_SURF_1_A = 0x7019C;
constexpr u32 PLANE_SURF_2_A = 0x7029C;
constexpr u32 PLANE_SURF_3_A = 0x7039C;
constexpr u32 PLANE_SURF_1_B = 0x7119C;
constexpr u32 PLANE_SURF_2_B = 0x7129C;
constexpr u32 PLANE_SURF_3_B = 0x7139C;
constexpr u32 PLANE_SURF_1_C = 0x7219C;
constexpr u32 PLANE_SURF_2_C = 0x7229C;
constexpr u32 PLANE_SURF_3_C = 0x7239C;

#endif  // VESPERAOS_DISPLAY_REGS_H
