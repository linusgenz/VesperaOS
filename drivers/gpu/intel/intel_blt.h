/**
 * @file intel_blt.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 16.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef VESPERAOS_INTEL_BLT_H
#define VESPERAOS_INTEL_BLT_H

#include "../../../kernel/graphics/IRenderDriver.h"
#include "../../pci/pci.h"
#include "graphics.h"
#include "kernel/addr.h"

struct KernelDevice;

// Force Wake Registers
#define FORCEWAKE_MT 0xA188          // Multi-threaded force wake control
#define FORCEWAKE_ACK 0x130044       // Force wake acknowledgment
#define FORCEWAKE_ENABLE 0x00010001  // Magic value to enable force wake

// Reset Control
#define RESET_CTL 0xD0          // Reset control register
#define RESET_BCS_BIT (1 << 1)  // BCS reset bit

// BCS Power Control
#define BCS_SWCTRL 0x22200      // Software control register
#define BCS_SWCTRL_WAKEUP 0x01  // Force wakeup bit

// GTT (Graphics Translation Table)
constexpr size_t GTT_OFFSET = 8ull * 1024 * 1024;    // 8MB offset from MMIO base
#define GTT_TOTAL_ENTRIES (256 * 1024)  // 256K entries = 1GB
#define GTT_START_INDEX 0x1000          // Start at entry 4096 (16MB)

// ============================================================================
// BCS Ring Buffer Registers (Base: 0x22000)
// ============================================================================

#define BCS_RING_BASE 0x22000
#define BCS_RING_TAIL 0x30   // Tail pointer (write position)
#define BCS_RING_HEAD 0x34   // Head pointer (read position)
#define BCS_RING_START 0x38  // Ring buffer start address
#define BCS_RING_CTL 0x3C    // Ring buffer control
#define BCS_HWSP 0x80        // Hardware status page address

// Ring Buffer Control Bits
#define RING_CTL_ENABLED 0x01    // Ring buffer enabled bit
#define RING_SIZE_MASK 0x1FF000  // Ring size mask (in bytes - 4096)

// ============================================================================
// GTT Entry Format (64-bit)
// ============================================================================

#define GTT_VALID 0x01ULL
#define GTT_PHYS_ADDR_MASK 0x000FFFFFFFFFF000ULL  // Bits [51:12]
#define GTT_PAT_SHIFT 2
#define GTT_PAT_MASK 0x3ULL

// PAT Memory Types (für GTT)
#define GTT_PAT_UC 0x0  // Uncached
#define GTT_PAT_WC 0x1  // Write-Combining
#define GTT_PAT_WT 0x2  // Write-Through
#define GTT_PAT_WB 0x3  // Write-Back (cached in LLC)

#define MOCS_UNCACHED 0x01        // UC für alle Caches
#define MOCS_LLC_ONLY 0x02        // Nur LLC, WB
#define MOCS_DISPLAY_BUFFER 0x03  // Für Display: LLC cacheable
#define MOCS_CACHED_WB 0x09       // L3 + LLC Write-Back (default für Texturen)

// ============================================================================
// MI (Memory Interface) Commands
// ============================================================================

#define MI_NOOP 0x00000000  // No operation

// MI_FLUSH_DW - Flush and write immediate data
#define MI_COMMAND_TYPE (0x0 << 29)
#define MI_FLUSH_DW_OPCODE (0x26 << 23)
#define MI_FLUSH_STORE_INDEX (1 << 21)  // Store to HWSP index
#define MI_FLUSH_POST_SYNC (1 << 14)    // Write immediate data
#define MI_FLUSH_DW_LEN 0x3             // Length field

#define MI_STORE_DATA_IMM_OPCODE (0x20 << 23)

#define HWSP_SEQNO_OFFSET_DWORDS 4
#define HWSP_SEQNO_OFFSET (HWSP_SEQNO_OFFSET_DWORDS + 16)

// ============================================================================
// XY_SRC_COPY_BLT Command - 2D Source Copy
// ============================================================================

#define XY_SRC_COPY_BLT_CMD (2u << 29)        // Command type: 2D processor
#define XY_SRC_COPY_BLT_OPCODE (0x53u << 22)  // Opcode: XY_SRC_COPY_BLT
#define XY_SRC_COPY_WRITE_RGB (1u << 20)      // Write RGB
#define XY_SRC_COPY_WRITE_ALPHA (1u << 21)    // Write Alpha
#define XY_SRC_TILING_ENABLE (1u << 15)       // Source tiling enable
#define XY_DEST_TILING_ENABLE (1u << 11)      // Destination tiling enable
#define XY_SRC_COPY_BLT_LEN 8                 // DWord length = 8 (10 DWords total)

// ============================================================================
// XY_COLOR_BLT Command - 2D Color Fill
// ============================================================================

#define XY_COLOR_BLT_CMD (2u << 29)          // Command type: 2D processor
#define XY_COLOR_BLT_OPCODE (0x50u << 22)    // Opcode: XY_COLOR_BLT
#define XY_COLOR_BLT_WRITE_RGB (1u << 20)    // Write RGB
#define XY_COLOR_BLT_WRITE_ALPHA (1u << 21)  // Write Alpha
#define XY_COLOR_TILING_ENABLE (1u << 11)
#define XY_COLOR_BLT_LEN 5  // DWord length = 5 (7 DWords total)

// BR13 - Color Depth
#define COLOR_DEPTH_8888 (0b11 << 24)  // 32-bit ARGB

// BR13 - Raster Operation
#define ROP_PATCOPY 0xF0  // Copy solid color to destination
#define ROP_SHIFT 16      // ROP position in BR13
#define PATCOPY (ROP_PATCOPY << ROP_SHIFT)

// Coordinate Masks
#define COORD_MASK 0xFFFF  // 16-bit coordinate mask
#define COORD_Y_SHIFT 16   // Y coordinate position

// ROP codes (must involve source, no pattern)
#define SRCCOPY 0xCC

// ============================================================================
// XY_MONO_SRC_COPY_BLT Command
// ============================================================================

#define XY_MONO_SRC_COPY_CMD 0x2 << 29        // Client: 2D Processor
#define XY_MONO_SRC_COPY_OPCODE 0x54 << 22    // Opcode: 0x54
#define XY_MONO_SRC_COPY_WRITE_ALPHA 1 << 21  // Write Alpha Channel
#define XY_MONO_SRC_COPY_WRITE_RGB 1 << 20    // Write RGB Channel
#define XY_MONO_SRC_COPY_LEN 0x08             // DWord Length: 8

// BR13 bits
#define MONO_SRC_TRANSPARENCY 1 << 29    // Transparency Enabled
#define MONO_SRC_USE_BACKGROUND 0 << 29  // Use Background

// ============================================================================
// XY_FAST_COPY_BLT Command
// ============================================================================

#define XY_FAST_COPY_BLT_CMD (2u << 29)        // 2D Processor
#define XY_FAST_COPY_BLT_OPCODE (0x42u << 22)  // XY_FAST_COPY_BLT

// DW0 tiling bits
#define FAST_SRC_TILING_LINEAR (0u << 20)
#define FAST_DST_TILING_LINEAR (0u << 13)

// Length: 8 DWORDs *after* DW0/1 → total 10 DWORDs
#define XY_FAST_COPY_BLT_LEN 8

// BR13
#define FAST_COLOR_DEPTH_8888 (0b011u << 24)  // 32bpp

// ============================================================================
// BAR0 Configuration
// ============================================================================

#define BAR0_ADDR_MASK ~0xFULL        // Mask to extract base address
constexpr size_t BAR0_SIZE = 16ull * 1024 * 1024;  // 16MB MMIO region

// ============================================================================
// Timing and Limits
// ============================================================================

#define RING_BUFFER_SIZE (64 * 1024)  // 64KB ring buffer
#define PAGE_SIZE 4096                // Standard page size
#define FORCE_WAKE_TIMEOUT 1000       // Force wake timeout iterations
#define RESET_DELAY 10000             // Reset delay iterations
#define IDLE_CHECK_DELAY 100          // Delay per idle check iteration
#define STATUS_LOG_INTERVAL 10000     // Status logging interval

// ============================================================================
// Display Plane Registers (Primary Plane A)
// ============================================================================

#define PLANE_CTL_1A 0x70180     // Plane control register
#define PLANE_STRIDE_1A 0x70188  // Stride (pitch) in bytes
#define PLANE_SIZE_1A 0x70190    // Size register
#define PLANE_POS_1A 0x7018C     // Position on screen
#define PLANE_OFFSET_1A 0x701A4  // Start offset in surface
#define PLANE_SURF_1A 0x7019C    // Surface address (triggers update)

// Plane Control Bits
#define PLANE_CTL_ENABLE (1 << 31)                // Enable plane
#define PLANE_CTL_PIPE_A (0 << 8)                 // Pipe A select
#define PLANE_CTL_FORMAT_XRGB8888 (0b0100 << 24)  // 32-bit XRGB format
#define PLANE_CTL_RGBX (1u << 20)                 // Sets the Color order to RGB
#define PLANE_CTL_TILE_Y (0b100 << 10)            // Enables Tile Y for the surface

enum class TileMode : uint8_t {
    Linear = 0,  // 256 KB alignment
    X = 1,       // 256 KB alignment
    Y = 2        // 1 MB alignment
};

struct GgttAllocation {
    virt_addr_t cpu_addr;
    uint64_t gfx_addr;
};

struct GpuFramebuffer {
    virt_addr_t cpu_addr;
    uint64_t gfx_addr;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    TileMode tile_mode;
};

struct GpuTextBuffer {
    virt_addr_t cpu_addr;
    uint64_t gfx_addr;
    uint32_t width;   // in pixels
    uint32_t height;  // in pixels
    size_t total_size;
};

struct BltRect {
    uint32_t x, y;
    uint32_t width, height;
};

class IntelBlt final : public IRenderDriver {
   public:
    explicit IntelBlt(PCI::PCIDeviceHeader* header);
    void start_device(uint32_t screen_width, uint32_t screen_height);

    bool fill_rect(uint32_t px, uint32_t py, uint32_t w, uint32_t h, uint32_t colour) override;

    bool blit_buffer(
        const void* pixels, uint32_t buffer_width, uint32_t buffer_height, uint32_t dst_x, uint32_t dst_y
    ) override;

    bool scroll_pixels(int dy) override;

    void draw_glyph_run(const GlyphRun& r) override {
        draw_str(r.text, r.px, r.py, r.fg, r.bg);
    }

    [[nodiscard]] KernelDevice* get_kd() const {
        return kd;
    }

    [[nodiscard]] uint32_t screen_width_px() const override;
    [[nodiscard]] uint32_t screen_height_px() const override;
    [[nodiscard]] uint32_t bytes_per_scanline() const override;

   private:
    KernelDevice* kd;

    volatile uint8_t* mmio_base;
    volatile uint32_t* bcs_regs;
    volatile uint64_t* gtt_entries{};

    uint64_t ring_graphics_addr;
    virt_addr_t ring_cpu_addr;
    uint32_t ring_size;
    uint32_t ring_tail{};

    virt_addr_t context_cpu_addr{};
    uint64_t context_graphics_addr{};

    uint64_t context_descriptor{};

    uint64_t hwsp_graphics_addr;
    virt_addr_t hwsp_cpu_addr;

    virt_addr_t pattern_buffer_cpu = make_virt(nullptr);
    uint64_t pattern_buffer_addr = 0;

    uint32_t gtt_next_free{};
    uint32_t gtt_total_entries{};

    uint32_t sequence_number;

    GpuTextBuffer text_buffer;
    GpuFramebuffer fb;

    void init_text_buffer(const FONT* font, uint32_t screen_width);

    void alloc_framebuffer(uint32_t width, uint32_t height, TileMode tile_mode);
    void build_text_scanline(const char* text, size_t length, FONT* font, uint8_t* buffer, size_t buffer_stride);
    bool draw_str(const char* text, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color);
    void xy_src_copy_blt(
        uint64_t dest_addr, uint32_t dest_pitch, uint32_t dest_x1, uint32_t dest_y1, uint32_t dest_x2, uint32_t dest_y2,
        uint64_t src_addr, uint32_t src_pitch, uint32_t src_x1, uint32_t src_y1
    );
    void xy_fast_copy_blt(
        uint64_t dest_addr, uint32_t dest_pitch, uint32_t dest_x1, uint32_t dest_y1, uint32_t dest_x2, uint32_t dest_y2,
        uint64_t src_addr, uint32_t src_pitch, uint32_t src_x1, uint32_t src_y1
    );

    void write_command(uint32_t cmd);
    void set_display_framebuffer() const;
    void mi_flush(uint32_t seqno);
    [[nodiscard]] bool wait_for_sequence(uint32_t target_seqno, uint32_t timeout_us) const;
    void flush_commands() const;
    void setup_ring_buffer();
    void enable_force_wake() const;
    void enable_bcs_power() const;
    void reset_bcs() const;
    void init_gtt();
    void emergency_reset_bcs();
    void check_gpu_health();
    bool validate_blt_params(const BltRect& rect) const;
    [[nodiscard]] bool wait_for_ring_space(uint32_t required_bytes, uint32_t timeout_us) const;
    void xy_color_blt(
        uint64_t dest_addr, uint32_t dest_pitch, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color
    );
    void xy_mono_src_copy_blt(
        uint64_t dest_addr, uint32_t dest_pitch, uint32_t dest_x1, uint32_t dest_y1, uint32_t dest_x2, uint32_t dest_y2,
        uint64_t mono_src_addr, uint32_t src_bit_position, bool transparency_enabled, uint32_t bg_color,
        uint32_t fg_color
    );
    GgttAllocation alloc_and_map_to_ggtt(size_t num_pages, uint64_t flags = 0, uint8_t pat_index = GTT_PAT_UC);
    uint64_t map_to_ggtt(uint64_t phys_addr, size_t num_pages, uint8_t pat_index);
};

#endif  // VESPERAOS_INTEL_BLT_H
