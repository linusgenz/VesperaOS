// intel_display.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 12.11.26.
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

#include "intel_display.h"

#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include <gpu/intel/core/intel_gpu_device.h>

#include "vespera/cpu/simd_mem.h"
#include "vespera/debug/chronos.h"

namespace gpu::intel::display {
    namespace {
        constexpr u32 FLIP_TIMEOUT_US = 50'000;

        constexpr u32 align_up(const u32 value, const u32 alignment) {
            return (value + alignment - 1) / alignment * alignment;
        }
    } // namespace

    IntelDisplay::IntelDisplay(core::IntelGpuDevice& device) : device_(device) {
    }

    bool IntelDisplay::init() {
        for (u32 i = 0; i < PIPE_COUNT; i++) {
            pipes_[i].init(device_, static_cast<PipeId>(i));
        }

        // Only pipe A drives the internal panel for now.
        pipe(PipeId::A).irq_init();
        log_firmware_state();

        if (!init_scanout()) {
            Log::error("intel-display: scanout setup failed, staying on firmware framebuffer");
            return false;
        }

        return true;
    }

    void IntelDisplay::log_firmware_state() {
        for (u32 i = 0; i < PLANES_PER_PIPE; i++) {
            PlaneConfig cfg{};
            if (plane(PipeId::A, static_cast<PlaneId>(i)).read_config(cfg)) {
                Log::info(
                    "intel-display: firmware pipe A plane %u: surf=0x%llx %ux%u pitch=%u tiling=%u",
                    i + 1,
                    gfx_raw(cfg.surface),
                    cfg.width,
                    cfg.height,
                    cfg.pitch_bytes,
                    static_cast<u32>(cfg.tiling)
                );
            }
        }
    }

    bool IntelDisplay::init_scanout() {
        PlaneConfig firmware_cfg{};
        if (!primary_plane().read_config(firmware_cfg)) {
            Log::error("intel-display: failed to read firmware plane 1 config");
            return false;
        }

        const Resolution res{firmware_cfg.width, firmware_cfg.height};
        if (!res.is_valid()) {
            Log::error(
                "intel-display: firmware plane 1 reports invalid resolution %ux%u",
                firmware_cfg.width, firmware_cfg.height
            );
            return false;
        }

        for (auto& buf : scanout_) {
            buf = alloc_scanout(res, TileMode::Linear);
            if (!buf.valid) {
                Log::error("intel-display: scanout buffer allocation failed");
                return false;
            }
        }

        const usize shadow_bytes = static_cast<usize>(scanout_[0].pitch) * scanout_[0].height;
        shadow_ = static_cast<u32*>(kernel::memory::malloc(shadow_bytes));
        if (!shadow_) {
            Log::error("intel-display: shadow buffer allocation failed");
            return false;
        }
        memset(shadow_, 0, shadow_bytes);
        shadow_stride_px_ = scanout_[0].pitch / SCANOUT_BYTES_PER_PIXEL;
        curr_dirty_.reset();
        prev_dirty_.reset();

        front_idx_ = 0;
        back_idx_ = 1;
        back_dirty_ = false;
        flip_pending_ = false;

        PlaneConfig cfg = scanout_[front_idx_].plane_config();
        cfg.format = firmware_cfg.format; // Pixelformat der Firmware übernehmen, Rest ist unser Buffer

        if (!primary_plane().commit(cfg)) {
            Log::error("intel-display: failed to commit scanout_[0] to plane 1");
            return false;
        }

        scanout_ready_ = true;
        Log::info(
            "intel-display: scanout ready %ux%u pitch=%u, double-buffered",
            res.width, res.height, scanout_[front_idx_].pitch
        );
        return true;
    }

    ScanoutBuffer IntelDisplay::alloc_scanout(const Resolution res, const TileMode tiling) const {
        ScanoutBuffer out{};

        if (!res.is_valid() || res.width > PLANE_MAX_WIDTH || res.height > PLANE_MAX_HEIGHT) {
            Log::error("intel-display: scanout: unsupported resolution %ux%u", res.width, res.height);
            return out;
        }

        const u32 pitch = align_up(res.width * SCANOUT_BYTES_PER_PIXEL, pitch_alignment_bytes(tiling));
        const u32 rows = align_up(res.height, tile_height_rows(tiling));

        const usize surface_bytes = static_cast<usize>(pitch) * rows;
        const usize surface_pages = (surface_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        const usize total_pages = surface_pages + SCANOUT_PTE_PADDING_PAGES;
        const u32 align_pages = surface_alignment_bytes(tiling) / PAGE_SIZE;

        const core::GgttAllocation alloc = device_.ggtt().alloc_persistent_aligned(
            total_pages, align_pages, (1ULL << PtFlag::ReadWrite), CacheMode::WriteCombining, core::MOCS_UNCACHED
        );
        if (virt_null(alloc.cpu_addr)) {
            Log::error("intel-display: scanout: GGTT allocation failed (%zu pages)", total_pages);
            return out;
        }

        memset(virt_ptr(alloc.cpu_addr), 0, total_pages * PAGE_SIZE);

        out.alloc = alloc;
        out.width = res.width;
        out.height = res.height;
        out.pitch = pitch;
        out.tiling = tiling;
        out.surface_pages = static_cast<u32>(surface_pages);
        out.total_pages = static_cast<u32>(total_pages);
        out.valid = true;

        Log::info(
            "intel-display: scanout %ux%u pitch=%u tiling=%u gfx=0x%llx (%zu pages + %u PTE padding)",
            res.width,
            res.height,
            pitch,
            static_cast<u32>(tiling),
            gfx_raw(alloc.gfx_addr),
            surface_pages,
            SCANOUT_PTE_PADDING_PAGES
        );

        return out;
    }

    bool IntelDisplay::handle_irq(const MASTER_INT_CTL& master) {
        bool handled = false;

        if (master.de_pipe_a_pending && pipe(PipeId::A).handle_irq()) {
            handled = true;
        }
        if (master.de_pipe_b_pending && pipe(PipeId::B).handle_irq()) {
            handled = true;
        }
        if (master.de_pipe_c_pending && pipe(PipeId::C).handle_irq()) {
            handled = true;
        }

        return handled;
    }

    void IntelDisplay::ensure_back_ready() {
        if (!flip_pending_) return;

        kernel::chronos::checkpoint("disp.flip_wait", "begin");
        if (!primary_plane().wait_flip_done(FLIP_TIMEOUT_US)) {
            Log::warning("intel-display: flip did not complete within %u us", FLIP_TIMEOUT_US);
        }
        flip_pending_ = false;
        kernel::chronos::checkpoint("disp.flip_wait", "flip_done");

        const ScanoutBuffer& front = scanout_[front_idx_];
        const ScanoutBuffer& back = scanout_[back_idx_];

        const usize bytes_to_copy = static_cast<usize>(front.pitch) * front.height;
        fb_memcpy_avx2(virt_ptr(back.alloc.cpu_addr), virt_ptr(front.alloc.cpu_addr), bytes_to_copy);
        kernel::chronos::checkpoint("disp.flip_wait", "back_sync_end");
    }

    bool IntelDisplay::fill_rect(const u32 px, const u32 py, const u32 w, const u32 h, const u32 colour) {
        if (!scanout_ready_ || !shadow_) return false;

        const ScanoutBuffer& back = scanout_[back_idx_];
        if (px > back.width || py > back.height) return false;
        if (w > back.width - px || h > back.height - py) return false;
        if (w == 0 || h == 0) return true;

        u32* first_row = shadow_ + static_cast<usize>(py) * shadow_stride_px_ + px;
        for (u32 x = 0; x < w; ++x) first_row[x] = colour;
        const usize row_bytes = static_cast<usize>(w) * SCANOUT_BYTES_PER_PIXEL;
        for (u32 y = 1; y < h; ++y) {
            memcpy(shadow_ + static_cast<usize>(py + y) * shadow_stride_px_ + px, first_row, row_bytes);
        }

        curr_dirty_.expand(px, py, w, h);
        return true;
    }

    bool IntelDisplay::blit_region(
        const u32* pixels, const u32 src_stride, const u32 src_x, const u32 src_y,
        const u32 w, const u32 h, const u32 dst_x, const u32 dst_y
    ) {
        if (!scanout_ready_ || !shadow_ || !pixels) return false;

        const ScanoutBuffer& back = scanout_[back_idx_];
        if (dst_x >= back.width || dst_y >= back.height) return false;

        const u32 max_w = (dst_x + w > back.width) ? back.width - dst_x : w;
        const u32 max_h = (dst_y + h > back.height) ? back.height - dst_y : h;
        if (max_w == 0 || max_h == 0) return true;

        kernel::chronos::checkpoint("disp.blit_region", "begin");

        const usize row_bytes = static_cast<usize>(max_w) * SCANOUT_BYTES_PER_PIXEL;
        for (u32 y = 0; y < max_h; ++y) {
            const u32* src_row = pixels + static_cast<usize>(src_y + y) * src_stride + src_x;
            u32* dst_row = shadow_ + static_cast<usize>(dst_y + y) * shadow_stride_px_ + dst_x;
            memcpy(dst_row, src_row, row_bytes); // normal cached memcpy -> bleibt im L3!
        }

        kernel::chronos::checkpoint("disp.blit_region", "memcopy done");

        curr_dirty_.expand(dst_x, dst_y, max_w, max_h);
        return true;
    }

    void IntelDisplay::upload_rect_to_back(const DirtyRect& r) const {
        if (!r.valid) return;
        const ScanoutBuffer& back = scanout_[back_idx_];
        auto* dst = static_cast<u32*>(virt_ptr(back.alloc.cpu_addr));
        const usize row_bytes = static_cast<usize>(r.x1 - r.x0) * SCANOUT_BYTES_PER_PIXEL;

        for (u32 y = r.y0; y < r.y1; ++y) {
            const usize off = static_cast<usize>(y) * shadow_stride_px_ + r.x0;
            fb_memcpy_avx2(dst + off, shadow_ + off, row_bytes);
        }
    }

    void IntelDisplay::present() {
        if (!scanout_ready_ || !curr_dirty_.valid) return;

        if (flip_pending_) {
            kernel::chronos::checkpoint("disp.flip_wait", "begin");
            if (!primary_plane().wait_flip_done(FLIP_TIMEOUT_US)) {
                Log::warning("intel-display: flip did not complete within %u us", FLIP_TIMEOUT_US);
            }
            flip_pending_ = false;
            kernel::chronos::checkpoint("disp.flip_wait", "flip_done");
        }

        kernel::chronos::checkpoint("disp.present", "present_begin");

        if (prev_dirty_.valid) {
            const DirtyRect u = curr_dirty_.union_with(prev_dirty_);
            if (u.area() <= curr_dirty_.area() + prev_dirty_.area()) {
                upload_rect_to_back(u);
            } else {
                upload_rect_to_back(prev_dirty_);
                upload_rect_to_back(curr_dirty_);
            }
        } else {
            upload_rect_to_back(curr_dirty_);
        }

        kernel::chronos::checkpoint("disp.present", "upload_done");

        DisplayPlane& plane1 = primary_plane();
        if (!plane1.flip(scanout_[back_idx_].alloc.gfx_addr)) {
            Log::error("intel-display: present: flip rejected");
            return;
        }

        const u32 new_back = front_idx_;
        front_idx_ = back_idx_;
        back_idx_ = new_back;

        prev_dirty_ = curr_dirty_;
        curr_dirty_.reset();
        flip_pending_ = true;

        kernel::chronos::checkpoint("disp.present", "flip_queued");
    }
} // namespace gpu::intel::display
