// intel_blt.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 16.12.25.
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
#ifndef VESPERAOS_INTEL_BLT_H
#define VESPERAOS_INTEL_BLT_H

#include <pci/pci_id.h>
#include <vespera/devices/device_info.h>
#include <vespera/graphics/IRenderDriver.h>
#include <vespera/interrupts.h>
#include <vespera/mm/addr.h>
#include <vespera/sync/atomic.h>

#include "../gpu_blt_queue.h"
#include "bcs_regs.h"
#include "ggtt_allocator.h"
#include "pci_config_regs.h"

namespace pci {
    struct pci_device;
}
struct KernelDevice;

// =========================================================================
// MMIO post-write barrier — forces a read-back to flush the write buffer.
// =========================================================================

#define MMIO_POST_WRITE(reg)            \
    do {                                \
        volatile u32 __tmp = (reg).raw; \
        (void)__tmp;                    \
    } while (0)

namespace blt {

    // =========================================================================
    // ForceWake
    // =========================================================================
    constexpr u32 FORCEWAKE_MT = 0xA188;
    constexpr u32 FORCEWAKE_ACK = 0x130044;
    constexpr u32 FORCEWAKE_ENABLE = 0x00010001;

    // =========================================================================
    // GTT / GGTT
    // =========================================================================
    constexpr usize GTT_OFFSET = 8ull * 1024 * 1024;
    constexpr u32 GTT_TOTAL_ENTRIES = 256u * 1024u;

    constexpr u64 GTT_VALID = 0x01ULL;
    constexpr u64 GTT_PHYS_ADDR_MASK = 0x000FFFFFFFFFF000ULL;
    constexpr u32 GTT_PAT_SHIFT = 2;
    constexpr u64 GTT_PAT_MASK = 0x3ULL;

    // PAT memory types
    constexpr u8 GTT_PAT_UC = 0x0;
    constexpr u8 GTT_PAT_WC = 0x1;
    constexpr u8 GTT_PAT_WT = 0x2;
    constexpr u8 GTT_PAT_WB = 0x3;

    // MOCS indices
    constexpr u8 MOCS_UNCACHED = 0x01;
    constexpr u8 MOCS_LLC_ONLY = 0x02;
    constexpr u8 MOCS_DISPLAY_BUFFER = 0x03;
    constexpr u8 MOCS_CACHED_WB = 0x09;

    // =========================================================================
    // BCS Ring Buffer Registers (byte offsets from BCS_RING_BASE)
    // =========================================================================

    constexpr u32 RING_CTL_ENABLED = 0x01;
    constexpr u32 RING_SIZE_MASK = 0x1FF000;

    // =========================================================================
    // HWSP
    // =========================================================================
    constexpr u32 HWSP_SEQNO_OFFSET_DWORDS = 4;
    constexpr u32 HWSP_SEQNO_OFFSET = HWSP_SEQNO_OFFSET_DWORDS + 16;

    // =========================================================================
    // BLT Command DWord Lengths
    // =========================================================================
    constexpr u32 XY_SRC_COPY_BLT_LEN = 8;
    constexpr u32 XY_COLOR_BLT_LEN = 5;
    constexpr u32 XY_MONO_SRC_COPY_LEN = 0x08;
    constexpr u32 XY_FAST_COPY_BLT_LEN = 8;
    constexpr u32 MI_FLUSH_DW_LEN = 0x3;

    // =========================================================================
    // ROP Codes
    // =========================================================================
    constexpr u8 ROP_PATCOPY = 0xF0;
    constexpr u8 SRCCOPY = 0xCC;

    // =========================================================================
    // BAR0 / MMIO
    // =========================================================================
    constexpr u64 GTTMMADR_ADDR_MASK = ~0xFULL;
    constexpr usize BAR0_SIZE = 16ull * 1024 * 1024;

    // =========================================================================
    // Timing and Limits
    // =========================================================================
    constexpr u32 RING_BUFFER_SIZE = 64u * 1024u;
    constexpr u32 FORCE_WAKE_TIMEOUT = 1000;
    constexpr u32 BCS_RESET_TIMEOUT = 1000;

    // =========================================================================
    // Data Types
    // =========================================================================
    constexpr usize BYTES_PER_PIXEL = 4;

    enum class TileMode : u8 {
        Linear = 0,
        X = 1,
        Y = 2,
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

    struct BltRect {
        u32 x;
        u32 y;
        u32 width;
        u32 height;
    };

    struct ScratchBuffer {
        GgttAllocation alloc{};
        u32 num_pages = 0;
        u32 max_w = 0;
        u32 max_h = 0;
        u32 pitch = 0;
        bool valid = false;
    };

    // =========================================================================
    // IntelBlt
    // =========================================================================

    class IntelBlt final : public IRenderDriver, public IDeviceInfo {
       public:
        explicit IntelBlt(const pci::pci_device& igpu_dev);

        IntelBlt(const IntelBlt&) = delete;
        IntelBlt& operator=(const IntelBlt&) = delete;

        bool init_device();

        void start_device(u32 screen_width, u32 screen_height);

        // IRenderDriver
        bool fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) override;
        bool blit_region(
            const u32* pixels, u32 src_stride, u32 src_x, u32 src_y, u32 w, u32 h, u32 dst_x, u32 dst_y
        ) override;
        void present() override;

        [[nodiscard]] u32 screen_width_px() const override;
        [[nodiscard]] u32 screen_height_px() const override;
        [[nodiscard]] u32 bytes_per_scanline() const override;

        // IDeviceInfo
        bool get_vendor(char* out, usize len) override;
        bool get_model(char* out, usize len) override;

        [[nodiscard]] KernelDevice* get_kd() const {
            return kd_;
        }

       private:
        static constexpr u32 SEQNO_BIT5_MASK = 1u << 5;
        static constexpr u32 RING_SPACE_FOR_FILL = 12 * 4 + 64;
        static constexpr u32 RING_SPACE_FOR_BLIT = 30 * 4 + 64;

        static constexpr u32 lo32(u64 v) {
            return static_cast<u32>(v);
        }
        static constexpr u32 hi32(u64 v) {
            return static_cast<u32>(v >> 32);
        }

        volatile INTEL_IGP_PCI_CONFIG* igp_cfg_;

        KernelDevice* kd_ = nullptr;
        u64 error_count_ = 0;
        u8 irq_vector_ = 0;

        pci::pci_id pci_id_;

        volatile u8* mmio_base_ = nullptr;
        volatile BCS_REGS* bcs_regs_ = nullptr;
        volatile u64* gtt_entries_ = nullptr;

        GgttAllocator ggtt_alloc_;

        gfx_addr_t ring_gfx_addr_ = {};
        virt_addr_t ring_cpu_addr_ = {};
        u32 ring_size_ = RING_BUFFER_SIZE;
        u32 ring_tail_ = 0;

        u32 last_head_ = 0;
        u32 hang_counter_ = 0;

        gfx_addr_t hwsp_gfx_addr_ = {};
        virt_addr_t hwsp_cpu_addr_ = {};

        u32 sequence_number_ = 0;

        GpuFramebuffer fb_{};
        GpuFramebuffer fb_back_{};
        ScratchBuffer scratch_{};

        AtomicFlag vblank_flag_;
        bool flip_pending_ = false;

        AtomicFlag completion_flag_;
        GpuBltQueue blt_queue_;

        // Worker
        void worker_start(u8 cpu_id);
        static void worker_entry(void* arg);
        bool execute_blit_region(const GpuBltRequest* req);
        bool execute_fill_rect(const GpuBltRequest* req);
        void execute_present();

        // Sequence numbers
        u32 seqno_next();
        bool seqno_wait(u32 target_seqno, u32 timeout_us);

        // Ring buffer
        void ring_write(u32 cmd);
        template <typename T>
        void ring_write_cmd(const T& cmd);
        void ring_flush();
        [[nodiscard]] bool ring_wait_space(u32 required_bytes, u32 timeout_us) const;
        void ring_init();

        // BCS command emission
        void emit_xy_color_blt(gfx_addr_t dest, u32 pitch, u32 x1, u32 y1, u32 x2, u32 y2, u32 color);
        void emit_xy_src_copy_blt(
            gfx_addr_t dest, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2, gfx_addr_t src,
            u32 src_pitch, u32 src_x1, u32 src_y1
        );
        void emit_xy_fast_copy_blt(
            gfx_addr_t dest, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2, gfx_addr_t src,
            u32 src_pitch, u32 src_x1, u32 src_y1
        );
        void emit_xy_mono_src_copy_blt(
            gfx_addr_t dest, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2, gfx_addr_t mono_src,
            u32 src_bit_pos, bool transparency, u32 bg_color, u32 fg_color
        );
        void emit_mi_flush(u32 seqno);

        // HW init / power
        bool force_wake_enable() const;
        void bcs_power_enable() const;
        bool bcs_reset() const;
        void bcs_interrupts_enable() const;
        void de_interrupts_enable() const;
        void bcs_error_reporting_init() const;
        bool bcs_emergency_reset();
        void gpu_health_check();

        // Framebuffer / scratch
        void fb_alloc(u32 width, u32 height, TileMode tile_mode);
        void fb_alloc_back();
        void fb_set_display() const;
        void scratch_init();

        [[nodiscard]] bool validate_rect(const BltRect& rect) const;

        static Irqreturn bcs_irq_handler(IntelBlt* self);
        static u32 tile_mode_to_tiling(TileMode mode);
        template <class T>
        T mmio_read(u32 reg) const;
        template <class T>
        void mmio_write(u32 reg, T val) const;

        // GGTT management, implemented in intel_blt_ggtt.cpp
        void ggtt_init();
        void ggtt_write_entries(u32 gtt_index, phys_addr_t phys_addr, usize num_pages, u8 pat_index) const;
        void ggtt_clear_entries(u32 gtt_index, usize num_pages) const;
        [[nodiscard]] GgttAllocation ggtt_alloc_persistent(usize num_pages, u64 flags = 0, u8 pat_index = GTT_PAT_UC);
        [[nodiscard]] GgttAllocation ggtt_alloc_transient(usize num_pages, u64 flags, u8 pat_index);
        void ggtt_free_transient(const GgttAllocation& alloc, usize num_pages);
    };

}  // namespace blt

#endif  // VESPERAOS_INTEL_BLT_H