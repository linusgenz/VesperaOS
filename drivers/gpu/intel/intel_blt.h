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

#include <vespera/devices/device_info.h>
#include <vespera/graphics/psf.h>
#include <vespera/mm/addr.h>

#include <kernel/graphics/IRenderDriver.h>
#include <drivers/pci/pci.h>
#include "ggtt_allocator.h"

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
constexpr usize GTT_OFFSET = 8ull * 1024 * 1024;  // 8MB offset from MMIO base
#define GTT_TOTAL_ENTRIES (256 * 1024)            // 256K entries = 1GB
#define GTT_START_INDEX 0x1000                    // Start at entry 4096 (16MB)

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

#define MI_NOOP 0x00000000  // No operation

#define HWSP_SEQNO_OFFSET_DWORDS 4
#define HWSP_SEQNO_OFFSET (HWSP_SEQNO_OFFSET_DWORDS + 16)

#define XY_SRC_COPY_BLT_LEN 8    // DWord length = 8 (10 DWords total)
#define XY_COLOR_BLT_LEN 5  // DWord length = 5 (7 DWords total)
#define XY_MONO_SRC_COPY_LEN 0x08             // DWord Length: 8
#define XY_FAST_COPY_BLT_LEN 8 // Length: 8 DWORDs *after* DW0/1 → total 10 DWORDs
#define MI_FLUSH_DW_LEN 0x3             // Length field

// BR13 - Raster Operation
#define ROP_PATCOPY 0xF0  // Copy solid color to destination

// Coordinate Masks
#define COORD_MASK 0xFFFF  // 16-bit coordinate mask

// ROP codes (must involve source, no pattern)
#define SRCCOPY 0xCC

// ============================================================================
// BAR0 Configuration
// ============================================================================

#define BAR0_ADDR_MASK ~0xFULL                    // Mask to extract base address
constexpr usize BAR0_SIZE = 16ull * 1024 * 1024;  // 16MB MMIO region

// ============================================================================
// Timing and Limits
// ============================================================================

#define RING_BUFFER_SIZE (64 * 1024)  // 64KB ring buffer
#define FORCE_WAKE_TIMEOUT 1000       // Force wake timeout iterations
#define RESET_DELAY 10000             // Reset delay iterations
#define IDLE_CHECK_DELAY 100          // Delay per idle check iteration
#define STATUS_LOG_INTERVAL 10000     // Status logging interval

// ============================================================================
// Display Plane Registers (Primary Plane A)
// ============================================================================

#define PLANE_CTL_1_A 0x70180     // Plane control register
#define PLANE_STRIDE_1_A 0x70188  // Stride (pitch) in bytes
#define PLANE_SIZE_1_A 0x70190    // Size register
#define PLANE_POS_1_A 0x7018C     // Position on screen
#define PLANE_OFFSET_1_A 0x701A4  // Start offset in surface
#define PLANE_SURF_1_A 0x7019C    // Surface address (triggers update)

// Plane Control Bits
#define PLANE_CTL_ENABLE (1 << 31)                // Enable plane
#define PLANE_CTL_PIPE_A (0 << 8)                 // Pipe A select
#define PLANE_CTL_FORMAT_XRGB8888 (0b0100 << 24)  // 32-bit XRGB format
#define PLANE_CTL_RGBX (1u << 20)                 // Sets the Color order to RGB
#define PLANE_CTL_TILE_Y (0b100 << 10)            // Enables Tile Y for the surface

namespace blt {
    enum class TileMode : u8 {
        Linear = 0,  // 256 KB alignment
        X = 1,       // 256 KB alignment
        Y = 2        // 1 MB alignment
    };

    struct GgttAllocation {
        virt_addr_t cpu_addr;
        gfx_addr_t gfx_addr;
    };

    struct GpuFramebuffer {
        virt_addr_t cpu_addr;
        gfx_addr_t gfx_addr;
        u32 width;
        u32 height;
        u32 bpp;
        u32 pitch;
        TileMode tile_mode;
    };

    struct GpuTextBuffer {
        virt_addr_t cpu_addr;
        gfx_addr_t gfx_addr;
        u32 width;   // in pixels
        u32 height;  // in pixels
        usize total_size;
    };

    struct BltRect {
        u32 x, y;
        u32 width, height;
    };

    class IntelBlt final : public IRenderDriver, public IDeviceInfo {
       public:
        explicit IntelBlt(pci::PCI_DEVICE_HEADER* header);
        void start_device(u32 screen_width, u32 screen_height);
        static u32 tile_mode_to_blt_flag(TileMode mode);

        bool fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) override;

        bool blit_buffer(const void* pixels, u32 buffer_width, u32 buffer_height, u32 dst_x, u32 dst_y) override;

        bool scroll_pixels(int dy) override;

        [[nodiscard]] KernelDevice* get_kd() const {
            return kd_;
        }

        [[nodiscard]] u32 screen_width_px() const override;
        [[nodiscard]] u32 screen_height_px() const override;
        [[nodiscard]] u32 bytes_per_scanline() const override;

        bool get_vendor(char* out, usize len) override;
        bool get_model(char* out, usize len) override;

       private:
        pci::PCI_HEADER0* pci_header_;

        KernelDevice* kd_;

        volatile u8* mmio_base_;
        volatile u32* bcs_regs_;
        volatile u64* gtt_entries_{};

        GgttAllocator ggtt_alloc_;

        gfx_addr_t ring_gfx_addr_;
        virt_addr_t ring_cpu_addr_;
        u32 ring_size_;
        u32 ring_tail_{};

        gfx_addr_t hwsp_gfx_addr_;
        virt_addr_t hwsp_cpu_addr_;

        u32 gtt_next_free_{};
        u32 gtt_total_entries_{};

        u32 sequence_number_;

        GpuTextBuffer text_buffer_;
        GpuFramebuffer fb_;

        void init_text_buffer(const PsfFont* font, u32 screen_width);
        template <class T>
        void write_command_struct(const T& cmd);

        void alloc_framebuffer(u32 width, u32 height, TileMode tile_mode);
        void build_text_scanline(const char* text, usize length, PsfFont* font, u8* buffer, usize buffer_stride);
        bool draw_str(const char* text, u32 x, u32 y, u32 fg_color, u32 bg_color);
        void xy_src_copy_blt(
            gfx_addr_t dest_addr, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2,
            gfx_addr_t src_addr, u32 src_pitch, u32 src_x1, u32 src_y1
        );
        void xy_fast_copy_blt(
            gfx_addr_t dest_addr, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2,
            gfx_addr_t src_addr, u32 src_pitch, u32 src_x1, u32 src_y1
        );

        void write_command(u32 cmd);
        void set_display_framebuffer() const;
        void mi_flush(u32 seqno);
        [[nodiscard]] bool wait_for_sequence(u32 target_seqno, u32 timeout_us) const;
        void flush_commands() const;
        void setup_ring_buffer();
        void enable_force_wake() const;
        void enable_bcs_power() const;
        void reset_bcs() const;
        void init_gtt();
        void map_to_ggtt_at(u32 gtt_index, phys_addr_t phys_addr, usize num_pages, u8 pat_index) const;
        void unmap_from_ggtt(u32 gtt_index, usize num_pages) const;
        void emergency_reset_bcs();
        void check_gpu_health();
        [[nodiscard]] bool validate_blt_params(const BltRect& rect) const;
        [[nodiscard]] bool wait_for_ring_space(u32 required_bytes, u32 timeout_us) const;
        void xy_color_blt(gfx_addr_t dest_addr, u32 dest_pitch, u32 x1, u32 y1, u32 x2, u32 y2, u32 color);
        void xy_mono_src_copy_blt(
            gfx_addr_t dest_addr, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2,
            gfx_addr_t mono_src_addr, u32 src_bit_position, bool transparency_enabled, u32 bg_color, u32 fg_color
        );
        GgttAllocation alloc_and_map_to_ggtt(usize num_pages, u64 flags = 0, u8 pat_index = GTT_PAT_UC);
        GgttAllocation alloc_and_map_to_ggtt_transient(usize num_pages, u64 flags, u8 pat_index);
        void free_ggtt_transient(const GgttAllocation& alloc, usize num_pages);
        gfx_addr_t map_to_ggtt(phys_addr_t phys_addr, usize num_pages, u8 pat_index);
    };
}  // namespace blt
#endif  // VESPERAOS_INTEL_BLT_H
