// intel_bcs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 16.12.25.
// Renamed from intel_blt.h / IntelBlt on 06.08.26 when RCS was introduced as
// a peer engine (see intel_gpu_device.h, intel_engine.h).
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
#ifndef VESPERAOS_INTEL_BCS_H
#define VESPERAOS_INTEL_BCS_H

#include <vespera/devices/device_info.h>
#include <vespera/graphics/IRenderDriver.h>
#include <vespera/interrupts.h>
#include <vespera/mm/addr.h>
#include <vespera/sync/atomic.h>

#include "../gpu_blt_queue.h"
#include "bcs_regs.h"
#include "gt_interrupt_regs.h"
#include "intel_engine.h"
#include "intel_forcewake.h"

struct KernelDevice;

namespace blt {

    // =========================================================================
    // ForceWake — Blitter domain (RCS has its own FORCEWAKE_RENDER pair, see
    // intel_rcs.h; the ack bit itself is shared, see intel_forcewake.h).
    // =========================================================================
    constexpr u32 FORCEWAKE_BLITTER = 0xA188;
    constexpr u32 FORCEWAKE_ACK_BLITTER = 0x130044;
    constexpr u32 FORCEWAKE_BLITTER_ENABLE = 0x00010001;
    constexpr u32 FORCEWAKE_BLITTER_TIMEOUT = 1000;

    // =========================================================================
    // BCS Ring Buffer Registers (byte offsets from BCS_RING_BASE)
    // =========================================================================
    constexpr u32 RING_CTL_ENABLED = 0x01;
    constexpr u32 RING_SIZE_MASK = 0x1FF000;

    // =========================================================================
    // HWSP
    // =========================================================================
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
    // Timing and Limits
    // =========================================================================
    constexpr u32 RING_BUFFER_SIZE = 64u * 1024u;
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
    // IntelBcs — Blitter Command Streamer engine.
    //
    // Peer of IntelRcs: both inherit IntelEngine and borrow the same
    // IntelGpuDevice for MMIO base and GGTT. Owns 2D blit/copy/fill command
    // emission, the display framebuffer/scratch buffers, and the present
    // path — none of which RCS needs or shares.
    // =========================================================================

    class IntelBcs final : public IntelEngine, public IRenderDriver, public IDeviceInfo {
       public:
        explicit IntelBcs(IntelGpuDevice& device);

        IntelBcs(const IntelBcs&) = delete;
        IntelBcs& operator=(const IntelBcs&) = delete;

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

        // IntelEngine hooks — see intel_engine.h. Registered with
        // IntelGpuDevice's shared GT0 dispatcher in init_device() via
        // device().register_engine_for_irq(this).
        [[nodiscard]] u32 gt_user_irq_bit() const override {
            return GT0_BCS_USER_IRQ_BIT;
        }
        void on_gt_user_interrupt() override {
            completion_flag_.set();
        }

        bool blit_gpu_surface(gfx_addr_t src_gfx, u32 src_pitch, u32 width, u32 height);

       private:
        static constexpr u32 RING_SPACE_FOR_FILL = 12 * 4 + 64;
        static constexpr u32 RING_SPACE_FOR_BLIT = 30 * 4 + 64;

        static constexpr u32 lo32(u64 v) {
            return static_cast<u32>(v);
        }
        static constexpr u32 hi32(u64 v) {
            return static_cast<u32>(v >> 32);
        }

        // BCS_RING_BASE (0x22000) is passed as this engine's MMIO offset to
        // IntelEngine's constructor; bcs_regs_ is engine_regs() reinterpreted
        // as the full BCS register block for BCS-specific registers
        // (SWCTRL, EIR/EMR, ...) that IntelEngine doesn't know about.
        volatile BCS_REGS* bcs_regs_ = nullptr;

        KernelDevice* kd_ = nullptr;

        u32 last_head_ = 0;
        u32 hang_counter_ = 0;

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
        void bcs_power_enable() const;
        void bcs_interrupts_enable() const;
        void bcs_error_reporting_init() const;
        bool bcs_emergency_reset();
        void gpu_health_check();

        // Framebuffer / scratch
        void fb_alloc(u32 width, u32 height, TileMode tile_mode);
        void fb_alloc_back();
        void fb_set_display() const;
        void scratch_init();

        [[nodiscard]] bool validate_rect(const BltRect& rect) const;

        /// Callback registered with device().register_de_pipe_a_handler().
        /// Replaces the DE-Pipe-A half of the old bcs_irq_handler — vblank
        /// and plane-1-flip-done are still IntelBcs' business (flip_pending_
        /// state, vblank_flag_), just invoked from IntelGpuDevice's shared
        /// dispatcher now instead of from a BCS-owned MSI handler.
        static void on_de_pipe_a_interrupt(void* ctx, bool vblank, bool plane1_flip_done);

        static u32 tile_mode_to_tiling(TileMode mode);
    };

}  // namespace blt

#endif  // VESPERAOS_INTEL_BCS_H
