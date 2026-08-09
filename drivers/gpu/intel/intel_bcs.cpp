// intel_bcs.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 16.12.25.
// Renamed from intel_blt.cpp / IntelBlt on 06.08.26 when RCS was introduced
// as a peer engine.
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

#include "intel_bcs.h"

#include <filesystem/devfs.h>
#include <klib/string.h>
#include <pci/msix.h>
#include <pci/pci.h>
#include <pci/pci_device.h>
#include <vespera/devices/device_manager.h>
#include <vespera/interrupts.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/realm/realm_types.h>
#include <vespera/time.h>
#include <vespera/unit/unit_manager.h>
#include <vespera/unit_config.h>
#include <drivers/mmio_post_write.h>

#include "blt_commands.h"
#include "display_regs.h"
#include "error_regs.h"
#include "gt_interrupt_regs.h"
#include "gt_reset_regs.h"
#include "interrupt_regs.h"
#include "pci_config_regs.h"

namespace blt {
    // =========================================================================
    // Constructor / Init
    // =========================================================================

    IntelBcs::IntelBcs(IntelGpuDevice& device)
        : IntelEngine(EngineType::BCS,
                      device, BCS_RING_BASE,
                      ForceWakeDomain{
                          FORCEWAKE_BLITTER, FORCEWAKE_ACK_BLITTER, FORCEWAKE_BLITTER_ENABLE, FORCEWAKE_ACK_BIT,
                          FORCEWAKE_BLITTER_TIMEOUT
                      }
          )
          , bcs_regs_(reinterpret_cast<volatile BCS_REGS*>(engine_regs())) {
    }

    bool IntelBcs::init_device() {
        if (!engine_force_wake_enable()) return false;

        bcs_power_enable();
        if (!engine_reset()) return false;

        vblank_flag_.init();
        completion_flag_.init(false);

        if (!device().register_engine_for_irq(this)) {
            Log::log_dbc("intel-bcs: failed to register for GT interrupts");
            return false;
        }

        bcs_interrupts_enable();

        if (!device().register_de_pipe_a_handler(&IntelBcs::on_de_pipe_a_interrupt, this)) {
            Log::log_dbc("intel-bcs: failed to register DE Pipe A handler");
            return false;
        }

        bcs_error_reporting_init();

        hwsp_alloc();
        ring_alloc_and_init(RING_BUFFER_SIZE);

        char name[16];
        DeviceManager::alloc_unique_device_name("intel_bcs", name, sizeof(name));
        kd_ = DeviceManager::register_device(
            DeviceDescriptor{}
            .set_name(name)
            .set_type(DeviceType::Gpu)
            .set_class(DeviceClass::Graphics)
            .set_bus(BusType::Pci)
            .set_controller(ControllerType::IntelGpu)
            .with_gpu(this)
            .with_info(this)
        );

        DevFs::register_device(kd_);

        return true;
    }

    void IntelBcs::start_device(const u32 screen_width, const u32 screen_height) {
        fb_alloc(screen_width, screen_height, TileMode::Linear);
        fb_alloc_back();
        scratch_init();
        worker_start(1);
        fb_set_display();
    }

    // =========================================================================
    // Worker
    // =========================================================================

    void IntelBcs::worker_start(const u8 cpu_id) {
        blt_queue_.init();

        const UnitConfig cfg = {
            .name       = "intel-bcs",
            .cpu_id     = cpu_id,
            .priority   = 4,
            .stack_size = 0x10000,
            .is_user    = false,
        };

        UnitManager::create(kernel::realm::REALM_DRIVER, worker_entry, this, &cfg);
    }

    void IntelBcs::worker_entry(void* arg) {
        auto* bcs = static_cast<IntelBcs*>(arg);

        while (true) {
            GpuBltRequest* req = bcs->blt_queue_.dequeue_blocking();
            if (!req) break;

            switch (req->op) {
                case GpuBltOp::BlitRegion:
                    bcs->execute_blit_region(req);
                    kernel::memory::free(req->owned_pixels);
                    req->owned_pixels = nullptr;
                    break;
                case GpuBltOp::FillRect:
                    bcs->execute_fill_rect(req);
                    break;
                case GpuBltOp::Present:
                    bcs->execute_present();
                    break;
                default:
                    break;
            }

            delete req;
        }
    }

    // =========================================================================
    // BCS Command Emission
    // =========================================================================

    void IntelBcs::emit_xy_color_blt(gfx_addr_t dest, u32 pitch, u32 x1, u32 y1, u32 x2, u32 y2, u32 color) {
        XY_COLOR_BLT_CMD cmd{};

        cmd.dw0.client = CLIENT_2D_PROCESSOR;
        cmd.dw0.opcode = OPCODE_XY_COLOR_BLT;
        cmd.dw0.write_alpha = 1;
        cmd.dw0.write_rgb = 1;
        cmd.dw0.dword_len = XY_COLOR_BLT_LEN;

        cmd.dw1.color_depth = COLOR_DEPTH_32BPP;
        cmd.dw1.rop = ROP_PATCOPY;
        cmd.dw1.dest_pitch = pitch & 0xFFFF;

        cmd.dw2.x1 = x1;
        cmd.dw2.y1 = y1;
        cmd.dw3.x2 = x2;
        cmd.dw3.y2 = y2;

        cmd.dest_addr_lo = lo32(gfx_raw(dest));
        cmd.dest_addr_hi = hi32(gfx_raw(dest));
        cmd.solid_color = color;

        ring_write_cmd(cmd);
    }

    void IntelBcs::emit_xy_src_copy_blt(
        gfx_addr_t dest, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2, gfx_addr_t src,
        u32 src_pitch, u32 src_x1, u32 src_y1
    ) {
        XY_SRC_COPY_BLT_CMD cmd{};

        cmd.dw0.client = CLIENT_2D_PROCESSOR;
        cmd.dw0.opcode = OPCODE_XY_SRC_COPY_BLT;
        cmd.dw0.write_alpha = 1;
        cmd.dw0.write_rgb = 1;
        cmd.dw0.dword_len = XY_SRC_COPY_BLT_LEN;

        cmd.dw1.color_depth = COLOR_DEPTH_32BPP;
        cmd.dw1.rop = SRCCOPY;
        cmd.dw1.dest_pitch = dest_pitch & 0xFFFF;

        cmd.dw2.x1 = dest_x1;
        cmd.dw2.y1 = dest_y1;
        cmd.dw3.x2 = dest_x2;
        cmd.dw3.y2 = dest_y2;

        cmd.dest_addr_lo = lo32(gfx_raw(dest));
        cmd.dest_addr_hi = hi32(gfx_raw(dest));

        cmd.dw6.src_x1 = src_x1;
        cmd.dw6.src_y1 = src_y1;
        cmd.dw7.src_pitch = src_pitch & 0xFFFF;

        cmd.src_addr_lo = lo32(gfx_raw(src));
        cmd.src_addr_hi = hi32(gfx_raw(src));

        ring_write_cmd(cmd);
    }

    void IntelBcs::emit_xy_fast_copy_blt(
        gfx_addr_t dest, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2, gfx_addr_t src,
        u32 src_pitch, u32 src_x1, u32 src_y1
    ) {
        XY_FAST_COPY_BLT_CMD cmd{};

        cmd.dw0.client = CLIENT_2D_PROCESSOR;
        cmd.dw0.opcode = OPCODE_XY_FAST_COPY_BLT;
        cmd.dw0.src_tiling = tile_mode_to_tiling(fb_.tile_mode);
        cmd.dw0.dst_tiling = TILING_LINEAR;
        cmd.dw0.dword_len = XY_FAST_COPY_BLT_LEN;

        cmd.dw1.color_depth = FAST_COLOR_DEPTH_32BPP;
        cmd.dw1.dest_pitch = dest_pitch & 0x7FFF; // bit[15] MBZ

        cmd.dw2.x1 = dest_x1;
        cmd.dw2.y1 = dest_y1;
        cmd.dw3.x2 = dest_x2;
        cmd.dw3.y2 = dest_y2;

        cmd.dest_addr_lo = lo32(gfx_raw(dest));
        cmd.dest_addr_hi = hi32(gfx_raw(dest));

        cmd.dw6.src_x1 = src_x1;
        cmd.dw6.src_y1 = src_y1;
        cmd.dw7.src_pitch = src_pitch & 0x7FFF;

        cmd.src_addr_lo = lo32(gfx_raw(src));
        cmd.src_addr_hi = hi32(gfx_raw(src));

        ring_write_cmd(cmd);
    }

    void IntelBcs::emit_xy_mono_src_copy_blt(
        gfx_addr_t dest, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2, gfx_addr_t mono_src,
        u32 src_bit_pos, bool transparency, u32 bg_color, u32 fg_color
    ) {
        XY_MONO_SRC_COPY_BLT_CMD cmd{};

        cmd.dw0.client = CLIENT_2D_PROCESSOR;
        cmd.dw0.opcode = OPCODE_XY_MONO_SRC_COPY_BLT;
        cmd.dw0.write_alpha = 1;
        cmd.dw0.write_rgb = 1;
        cmd.dw0.mono_src_bit_pos = src_bit_pos & 0x7;
        cmd.dw0.dword_len = XY_MONO_SRC_COPY_LEN;

        cmd.dw1.color_depth = COLOR_DEPTH_32BPP;
        cmd.dw1.rop = SRCCOPY;
        cmd.dw1.transparency = transparency ? 1u : 0u;
        cmd.dw1.dest_pitch = dest_pitch & 0xFFFF;

        cmd.dw2.x1 = dest_x1;
        cmd.dw2.y1 = dest_y1;
        cmd.dw3.x2 = dest_x2;
        cmd.dw3.y2 = dest_y2;

        cmd.dest_addr_lo = static_cast<u32>(gfx_raw(dest) & 0xFFFFFFFF);
        cmd.dest_addr_hi = static_cast<u32>(gfx_raw(dest) >> 32);
        cmd.mono_src_addr_lo = static_cast<u32>(gfx_raw(mono_src) & 0xFFFFFFFF);
        cmd.mono_src_addr_hi = static_cast<u32>(gfx_raw(mono_src) >> 32);
        cmd.bg_color = bg_color;
        cmd.fg_color = fg_color;
        cmd.trailing = 0;

        ring_write_cmd(cmd);
    }

    void IntelBcs::emit_mi_flush(u32 seqno) {
        MI_FLUSH_DW_CMD cmd{};

        cmd.dw0.client = CLIENT_MI;
        cmd.dw0.opcode = OPCODE_MI_FLUSH_DW;
        cmd.dw0.store_index = 1;
        cmd.dw0.post_sync = 1;
        cmd.dw0.dword_len = MI_FLUSH_DW_LEN;

        cmd.dw1.addr_lo = HWSP_SEQNO_OFFSET >> 2;
        cmd.dw2.addr_hi = 0;
        cmd.immediate_data = seqno;

        // IHD-OS-KBL-Vol 2a (p. 993): bit 5 of the write data must be clear.
        if (cmd.immediate_data & (1u << 5)) {
            kernel::SystemManager::system_panic("intel-bcs: MI_FLUSH_DW seqno has bit 5 set", DRVERR);
        };

        ring_write_cmd(cmd);

        MI_USER_INTERRUPT_CMD ui{};
        ui.opcode = OPCODE_MI_USER_INTERRUPT;
        ui.client = CLIENT_MI;
        ring_write_cmd(ui);
    }

    // =========================================================================
    // IRQ
    // =========================================================================

    // GT0/BCS user-interrupt handling moved to IntelBcs::on_gt_user_interrupt()
    // (called from IntelGpuDevice's shared GT0 dispatcher — see
    // intel_engine.h / intel_gpu_device.cpp). Only the DE Pipe A half
    // (vblank / flip-done, BCS-specific flip_pending_ state) remains here.
    void IntelBcs::on_de_pipe_a_interrupt(void* ctx, bool vblank, bool plane1_flip_done) {
        auto* self = static_cast<IntelBcs*>(ctx);

        if ((vblank || plane1_flip_done) && self->flip_pending_) {
            self->flip_pending_ = false;
            self->device().de_pipe_a_disarm_vblank();
            self->vblank_flag_.set();
        }
    }

    // =========================================================================
    // HW Init / Power
    // =========================================================================

    void IntelBcs::bcs_power_enable() const {
        BCS_SWCTRL swctrl;
        swctrl.raw = bcs_regs_->swctrl.raw;
        swctrl.tile_y_src = 1;
        bcs_regs_->swctrl.raw = swctrl.raw;
        MMIO_POST_WRITE(bcs_regs_->swctrl);
    }

    void IntelBcs::bcs_interrupts_enable() const {
        volatile BCS_IMR_REG& ring_imr = bcs_regs_->imr;

        BCS_IMR_REG bcs_imr{};
        bcs_imr.bits.user_irq = 0;
        bcs_imr.bits.master_error = 1;
        bcs_imr.bits.mi_flush_dw = 1;
        bcs_imr.bits.timeout = 1;
        bcs_imr.bits.ctx_switch = 1;
        bcs_imr.bits.wait_sem = 1;
        ring_imr.raw = bcs_imr.raw;
        MMIO_POST_WRITE(ring_imr);
    }

    void IntelBcs::bcs_error_reporting_init() const {
        volatile EIR_REG& eir = bcs_regs_->eir;
        EIR_REG v{};
        v.bcs_error_bits.instruction_error = 0;
        v.bcs_error_bits.privilege_violation = 0;
        v.mask = 0xFFFFu;
        eir.raw = v.raw;
        MMIO_POST_WRITE(eir);

        volatile EMR_REG& emr = bcs_regs_->emr;
        EMR_REG emr_val{};
        emr_val.error_mask = 0x00;
        emr_val.reserved = 0xFFFFFFu;
        emr.raw = emr_val.raw;
        MMIO_POST_WRITE(emr);
    }

    bool IntelBcs::bcs_emergency_reset() {
        // disable ring
        RING_BUFFER_CTL ctl{};
        ctl.raw = bcs_regs_->ring_ctl.raw;
        ctl.ring_enable = 0;
        bcs_regs_->ring_ctl.raw = ctl.raw;

        if (!engine_reset()) return false;

        memset(virt_ptr(ring_cpu_addr_), 0, ring_size_);
        ring_alloc_and_init(ring_size_);

        ring_tail_ = 0;
        sequence_number_ = 0;

        return true;
    }

    void IntelBcs::gpu_health_check() {
        const u32 head = bcs_regs_->ring_head.head_offset_bytes();
        const u32 tail = bcs_regs_->ring_tail.tail_offset_bytes();
        const u32 ctl = bcs_regs_->ring_ctl.raw;

        if (!(ctl & RING_CTL_ENABLED)) {
            error_count_++;
            if (!bcs_emergency_reset()) {
                Log::error("intel-bcs: emergency reset failed - GPU unresponsive");
            }
            return;
        }

        if (head == last_head_ && head != tail) {
            if (++hang_counter_ > 1000) {
                error_count_++;
                if (!bcs_emergency_reset()) {
                    Log::error("intel-bcs: emergency reset failed - GPU unresponsive");
                }
                hang_counter_ = 0;
            }
        } else {
            hang_counter_ = 0;
        }
        last_head_ = head;
    }

    // =========================================================================
    // Framebuffer / Scratch
    // =========================================================================

    void IntelBcs::fb_alloc(u32 width, u32 height, TileMode tile_mode) {
        fb_.width = width;
        fb_.height = height;
        fb_.bpp = BYTES_PER_PIXEL;
        fb_.tile_mode = tile_mode;

        if (tile_mode == TileMode::X) {
            fb_.pitch = ((width * fb_.bpp + 511) / 512) * 512;
        } else {
            fb_.pitch = ((width * fb_.bpp + 63) / 64) * 64;
        }

        const usize total_size = static_cast<usize>(fb_.pitch) * height;
        const usize num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

        Log::log_dbc(
            "intel-bcs: FB %dx%d pitch=%d size=%zu pages=%zu tiling=%u",
            width,
            height,
            fb_.pitch,
            total_size,
            num_pages,
            static_cast<u32>(tile_mode)
        );

        auto alloc = ggtt().alloc_persistent(num_pages, (1ULL << CacheDisabled), MOCS_UNCACHED);
        fb_.gfx_addr = alloc.gfx_addr;
        fb_.cpu_addr = alloc.cpu_addr;

        memset(virt_ptr(fb_.cpu_addr), 0, total_size);
    }

    void IntelBcs::fb_alloc_back() {
        const usize total_size = static_cast<usize>(fb_.pitch) * fb_.height;
        const usize num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

        auto alloc = ggtt().alloc_persistent(num_pages, (1ULL << CacheDisabled), MOCS_UNCACHED);
        fb_back_.width = fb_.width;
        fb_back_.height = fb_.height;
        fb_back_.bpp = fb_.bpp;
        fb_back_.pitch = fb_.pitch;
        fb_back_.tile_mode = fb_.tile_mode;
        fb_back_.gfx_addr = alloc.gfx_addr;
        fb_back_.cpu_addr = alloc.cpu_addr;

        memset(virt_ptr(fb_back_.cpu_addr), 0, total_size);

        Log::info("intel-bcs: back-buffer gfx=0x%llx (%zu pages)", gfx_raw(fb_back_.gfx_addr), num_pages);
    }

    void IntelBcs::scratch_init() {
        const usize row_bytes = static_cast<usize>(fb_.width) * BYTES_PER_PIXEL;
        const usize pitch = ((row_bytes + 63) / 64) * 64;
        const usize buf_size = pitch * fb_.height;
        const u32 num_pages = static_cast<u32>((buf_size + PAGE_SIZE - 1) / PAGE_SIZE);

        const GgttAllocation alloc = ggtt().alloc_persistent(num_pages, (1ULL << CacheDisabled), MOCS_UNCACHED);
        if (virt_null(alloc.cpu_addr)) {
            Log::error("intel-bcs: failed to alloc scratch buffer (%u pages)", num_pages);
            return;
        }

        memset(virt_ptr(alloc.cpu_addr), 0, num_pages * PAGE_SIZE);

        scratch_.alloc = alloc;
        scratch_.num_pages = num_pages;
        scratch_.max_w = fb_.width;
        scratch_.max_h = fb_.height;
        scratch_.pitch = static_cast<u32>(pitch);
        scratch_.valid = true;

        Log::info(
            "intel-bcs: scratch %u pages (%lu MB) for %ux%u @ pitch=%u",
            num_pages,
            (num_pages * PAGE_SIZE) >> 20,
            fb_.width,
            fb_.height,
            scratch_.pitch
        );
    }

    // https://kiwitree.net/~lina/intel-gfx-docs/prm/kbl/intel-gfx-prm-osrc-kbl-vol02c-commandreference-registers-part2_0.pdf
    // (p. 604)
    void IntelBcs::fb_set_display() const {
        // Disable plane before reconfiguring
        auto ctl = mmio_read<PLANE_CTL>(PLANE_CTL_1_A);
        ctl.plane_enable = 0;
        mmio_write(PLANE_CTL_1_A, ctl);
        kernel::time::sleep_us(1);

        ctl.raw = 0;
        ctl.source_pixel_format = PLANE_CTL::FMT_RGB_8_8_8_8;
        ctl.rgb_color_order = 0; // BGRX

        PLANE_STRIDE stride{};

        switch (fb_.tile_mode) {
            case TileMode::Linear:
                ctl.tiled_surface = PLANE_CTL::TILING_LINEAR;
                break;
            case TileMode::X:
                ctl.tiled_surface = PLANE_CTL::TILING_X;
                break;
            case TileMode::Y:
                ctl.tiled_surface = PLANE_CTL::TILING_Y;
                break;
            default:
                return;
        }

        stride.set_stride_bytes(fb_.pitch, ctl.tiled_surface);

        // Program plane registers (SURF must come last to arm the flip)
        mmio_write(PLANE_STRIDE_1_A, stride);

        PLANE_SIZE size{};
        size.set(fb_.width, fb_.height);
        mmio_write(PLANE_SIZE_1_A, size);

        PLANE_POS pos{};
        pos.set(0, 0);
        mmio_write(PLANE_POS_1_A, pos);

        PLANE_OFFSET offset{};
        offset.set(0, 0);
        mmio_write(PLANE_OFFSET_1_A, offset);

        mmio_write(PLANE_CTL_1_A, ctl);

        // Enable plane, takes effect at next VBlank
        ctl.plane_enable = 1;
        mmio_write(PLANE_CTL_1_A, ctl);

        // Write PLANE_SURF last, arms the double-buffer flip
        PLANE_SURF surf{};
        surf.set_address(static_cast<u32>(gfx_raw(fb_.gfx_addr)));
        mmio_write(PLANE_SURF_1_A, surf);

        asm volatile("mfence" ::: "memory");

        Log::log_dbc(
            "intel-bcs: display surface addr=0x%llx %dx%d pitch=%d tiling=%d",
            gfx_raw(fb_.gfx_addr),
            fb_.width,
            fb_.height,
            fb_.pitch,
            ctl.tiled_surface
        );
    }

    // =========================================================================
    // Validation / Health
    // =========================================================================

    bool IntelBcs::validate_rect(const BltRect& rect) const {
        if (virt_null(fb_.cpu_addr)) return false;
        if (rect.width == 0 || rect.height == 0) return false;
        if (rect.width > 8192 || rect.height > 4096) return false;
        if (rect.x + rect.width > fb_.width) return false;
        if (rect.y + rect.height > fb_.height) return false;
        if (gfx_raw(fb_.gfx_addr) & 0x3F) return false;
        if (fb_.pitch & 0x3F) return false;
        return true;
    }

    // =========================================================================
    // Public API — IRenderDriver
    // =========================================================================

    bool IntelBcs::fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) {
        auto* req = new GpuBltRequest();
        req->op = GpuBltOp::FillRect;
        req->dst_x = px;
        req->dst_y = py;
        req->w = w;
        req->h = h;
        req->color = colour;
        blt_queue_.submit(req);
        return true;
    }

    bool IntelBcs::blit_region(
        const u32* pixels, const u32 src_stride, const u32 src_x, const u32 src_y, const u32 w, const u32 h,
        const u32 dst_x, const u32 dst_y
    ) {
        if (!pixels) return false;
        if (dst_x >= fb_.width || dst_y >= fb_.height) return false;

        const u32 max_w = (dst_x + w > fb_.width) ? fb_.width - dst_x : w;
        const u32 max_h = (dst_y + h > fb_.height) ? fb_.height - dst_y : h;
        if (max_w == 0 || max_h == 0) return true;

        const usize row_bytes = static_cast<usize>(max_w) * BYTES_PER_PIXEL;
        auto* copy = static_cast<u8*>(kernel::memory::malloc(row_bytes * max_h));
        if (!copy) return false;

        const usize src_stride_bytes = static_cast<usize>(src_stride) * BYTES_PER_PIXEL;
        const u8* src_base = reinterpret_cast<const u8*>(pixels) + static_cast<usize>(src_y) * src_stride_bytes +
            static_cast<usize>(src_x) * BYTES_PER_PIXEL;

        for (u32 y = 0; y < max_h; y++) {
            memcpy(copy + y * row_bytes, src_base + y * src_stride_bytes, row_bytes);
        }

        auto* req = new GpuBltRequest();
        req->op = GpuBltOp::BlitRegion;
        req->owned_pixels = reinterpret_cast<u32*>(copy);
        req->src_stride = max_w;
        req->src_x = 0;
        req->src_y = 0;
        req->dst_x = dst_x;
        req->dst_y = dst_y;
        req->w = max_w;
        req->h = max_h;

        blt_queue_.submit(req);
        return true;
    }

    void IntelBcs::present() {
        auto* req = new GpuBltRequest();
        req->op = GpuBltOp::Present;
        blt_queue_.submit(req);
    }

    bool IntelBcs::blit_gpu_surface(gfx_addr_t src_gfx, u32 src_pitch, u32 width, u32 height) {
        gpu_health_check();
        if (!ring_wait_space(RING_SPACE_FOR_BLIT, 1'000'000)) return false;

        emit_xy_src_copy_blt(
            fb_back_.gfx_addr, fb_back_.pitch,
            0, 0, width, height,
            src_gfx, src_pitch,
            0, 0
        );

        const u32 target_seqno = seqno_next();
        emit_mi_flush(target_seqno);
        ring_flush();

        return seqno_wait(target_seqno, 500'000, completion_flag_);
    }

    // =========================================================================
    // Worker Execute Paths
    // =========================================================================

    bool IntelBcs::execute_blit_region(const GpuBltRequest* req) {
        if (req->w == 0 || req->h == 0) return true;

        if (!scratch_.valid) {
            Log::error("intel-bcs: scratch buffer not initialized");
            return false;
        }

        const usize row_bytes = static_cast<usize>(req->w) * BYTES_PER_PIXEL;
        auto src = reinterpret_cast<const u8*>(req->owned_pixels);
        u8* dst = virt_as<u8>(scratch_.alloc.cpu_addr);

        for (u32 y = 0; y < req->h; y++) {
            memcpy(dst + static_cast<usize>(y) * scratch_.pitch, src + static_cast<usize>(y) * row_bytes, row_bytes);
        }
        asm volatile("mfence" ::: "memory");

        gpu_health_check();
        if (!ring_wait_space(RING_SPACE_FOR_BLIT, 1'000'000)) return false;

        emit_xy_src_copy_blt(
            fb_back_.gfx_addr,
            fb_back_.pitch,
            req->dst_x,
            req->dst_y,
            req->dst_x + req->w,
            req->dst_y + req->h,
            scratch_.alloc.gfx_addr,
            scratch_.pitch,
            0,
            0
        );

        const u32 target_seqno = seqno_next();
        emit_mi_flush(target_seqno);
        ring_flush();

        return seqno_wait(target_seqno, 500'000, completion_flag_);
    }

    bool IntelBcs::execute_fill_rect(const GpuBltRequest* req) {
        const BltRect rect{.x = req->dst_x, .y = req->dst_y, .width = req->w, .height = req->h};
        if (!validate_rect(rect)) return false;

        gpu_health_check();
        if (!ring_wait_space(RING_SPACE_FOR_FILL, 1'000'000)) return false;

        emit_xy_color_blt(
            fb_back_.gfx_addr, fb_back_.pitch, rect.x, rect.y, rect.x + rect.width, rect.y + rect.height, req->color
        );

        const u32 target_seqno = seqno_next();
        emit_mi_flush(target_seqno);
        ring_flush();

        return seqno_wait(target_seqno, 5'000'000, completion_flag_);
    }

    void IntelBcs::execute_present() {
        PLANE_SURF surf{};
        surf.set_address(static_cast<u32>(gfx_raw(fb_back_.gfx_addr)));

        device().de_pipe_a_arm_vblank_oneshot();

        flip_pending_ = true;
        asm volatile("mfence" ::: "memory");
        mmio_write(PLANE_SURF_1_A, surf);
        asm volatile("mfence" ::: "memory");

        constexpr u32 total_required_bytes = RING_SPACE_FOR_BLIT + sizeof(MI_WAIT_FOR_EVENT_CMD);

        if (!ring_wait_space(total_required_bytes, 1'000'000)) {
            Log::log_dbc("intel-bcs: Timeout waiting for ring space in execute_present");
            return;
        }

        MI_WAIT_FOR_EVENT_CMD wait_cmd{};
        wait_cmd.client = CLIENT_MI;
        wait_cmd.opcode = OPCODE_MI_WAIT_FOR_EVENT;
        wait_cmd.display_plane_1_a_flip_pending_wait = 1;

        ring_write(wait_cmd.raw);

        klib::swap(fb_, fb_back_);

        emit_xy_src_copy_blt(
            fb_back_.gfx_addr, fb_back_.pitch, 0, 0, fb_back_.width, fb_back_.height, fb_.gfx_addr, fb_.pitch, 0, 0
        );

        const u32 seqno = seqno_next();
        emit_mi_flush(seqno);
        ring_flush();
        seqno_wait(seqno, 500'000, completion_flag_);
    }

    // =========================================================================
    // IRenderDriver Accessors
    // =========================================================================

    u32 IntelBcs::screen_width_px() const {
        return fb_.width;
    }

    u32 IntelBcs::screen_height_px() const {
        return fb_.height;
    }

    u32 IntelBcs::bytes_per_scanline() const {
        return fb_.pitch;
    }

    bool IntelBcs::get_vendor(char* out, const usize len) {
        strncpy(out, pci::get_vendor_name(device().pci_cfg()->vendor_id), len);
        out[len - 1] = '\0';
        return true;
    }

    bool IntelBcs::get_model(char* out, const usize len) {
        strncpy(out, pci::get_device_name(device().pci_cfg()->vendor_id, device().pci_cfg()->device_id), len);
        out[len - 1] = '\0';
        return true;
    }

    // =========================================================================
    // Utility
    // =========================================================================

    u32 IntelBcs::tile_mode_to_tiling(TileMode mode) {
        switch (mode) {
            case TileMode::X:
                return TILING_X;
            case TileMode::Y:
                return TILING_Y;
            default:
                return TILING_LINEAR;
        }
    }
} // namespace blt
