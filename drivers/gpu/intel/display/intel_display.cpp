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

#include "display_mmio.h"
#include "vespera/cpu/simd_mem.h"
#include "vespera/debug/chronos.h"
#include "vespera/sync/poll_table.h"

namespace gpu::intel::display {
    namespace {
        constexpr u32 FLIP_TIMEOUT_US = 50'000;

        constexpr u32 align_up(const u32 value, const u32 alignment) {
            return (value + alignment - 1) / alignment * alignment;
        }

        [[nodiscard]] constexpr CursorSize pick_cursor_size(const u32 width, const u32 height) {
            const u32 max_dim = (width > height) ? width : height;
            if (max_dim <= 64) return CursorSize::Size64;
            if (max_dim <= 128) return CursorSize::Size128;
            return CursorSize::Size256;
        }
    } // namespace

    IntelDisplay::IntelDisplay(core::IntelGpuDevice& device) : device_(device) {
    }

    bool IntelDisplay::init() {
        cursor_lock_.init();

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
                const auto wm0 = mmio_read<PLANE_WM>(
                    device_, plane_wm_addr(plane_reg(PipeId::A, static_cast<PlaneId>(i), PLANE_WM_1_A), 0)
                );
                Log::info(
                    "intel-display: firmware pipe A plane %u: surf=0x%llx %ux%u pitch=%u tiling=%u ddb=[%u..%u] wm0=(en=%u blk=%u)",
                    i + 1,
                    gfx_raw(cfg.surface),
                    cfg.width,
                    cfg.height,
                    cfg.pitch_bytes,
                    static_cast<u32>(cfg.tiling),
                    cfg.ddb.start,
                    cfg.ddb.end,
                    wm0.enable,
                    wm0.blocks
                );
            }
        }

        const auto cur_buf = mmio_read<PLANE_BUF_CFG>(device_, cursor_reg(PipeId::A, CUR_BUF_CFG_A));
        const auto cur_wm0 = mmio_read<PLANE_WM>(device_, plane_wm_addr(cursor_reg(PipeId::A, CUR_WM_A), 0));
        CursorConfig cur_cfg{};
        if (primary_cursor().read_config(cur_cfg)) {
            Log::info(
                "intel-display: firmware pipe A cursor: surf=0x%llx mode=0x%x pos=(%d,%d) ddb=[%u..%u] wm0=(en=%u blk=%u)",
                gfx_raw(cur_cfg.surface),
                static_cast<u32>(cur_cfg.mode),
                cur_cfg.x,
                cur_cfg.y,
                cur_buf.buffer_start,
                cur_buf.buffer_end,
                cur_wm0.enable,
                cur_wm0.blocks
            );
        } else {
            Log::info(
                "intel-display: firmware pipe A cursor: disabled, ddb_raw=0x%x wm0_raw=0x%x",
                cur_buf.raw,
                cur_wm0.raw
            );
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

        PlaneConfig cfg = scanout_[front_idx_].plane_config();
        cfg.format = firmware_cfg.format; // Pixelformat der Firmware übernehmen, Rest ist unser Buffer

        // Ensure cursor is disabled before partitioning DDB so no two planes
        // ever overlap in the Display Data Buffer across a VBlank.
        primary_cursor().disable();

        DdbAllocation cursor_ddb{};
        const auto fw_cur_buf = mmio_read<PLANE_BUF_CFG>(device_, cursor_reg(PipeId::A, CUR_BUF_CFG_A));
        const DdbAllocation fw_cur_ddb = (fw_cur_buf.raw != 0 && fw_cur_buf.buffer_end >= fw_cur_buf.buffer_start)
            ? DdbAllocation{static_cast<u16>(fw_cur_buf.buffer_start), static_cast<u16>(fw_cur_buf.buffer_end), true}
            : DdbAllocation{};

        const bool fw_cur_overlaps_plane1 = fw_cur_ddb.valid && firmware_cfg.ddb.valid &&
            !(fw_cur_ddb.end < firmware_cfg.ddb.start || fw_cur_ddb.start > firmware_cfg.ddb.end);

        if (fw_cur_ddb.valid && !fw_cur_overlaps_plane1 && fw_cur_ddb.blocks() >= CURSOR_DDB_BLOCKS) {
            cursor_ddb = fw_cur_ddb;
        } else if (firmware_cfg.ddb.valid && firmware_cfg.ddb.blocks() > CURSOR_DDB_BLOCKS + 32) {
            // Carve CURSOR_DDB_BLOCKS off the top of primary plane 1's DDB.
            cfg.ddb = {
                firmware_cfg.ddb.start,
                static_cast<u16>(firmware_cfg.ddb.end - CURSOR_DDB_BLOCKS),
                true,
            };
            cursor_ddb = {
                static_cast<u16>(firmware_cfg.ddb.end - CURSOR_DDB_BLOCKS + 1),
                firmware_cfg.ddb.end,
                true,
            };
        }

        if (!primary_plane().commit(cfg)) {
            Log::error("intel-display: failed to commit scanout_[0] to plane 1");
            return false;
        }

        if (cursor_ddb.valid) {
            // Wait for 1 VBlank so plane 1 has latched its shrunken DDB and
            // released the cursor slice before any cursor commit can arm it.
            if (!primary_plane().wait_flip_done(FLIP_TIMEOUT_US)) {
                Log::warning("intel-display: initial DDB partition flip wait timed out");
            }
            primary_cursor().set_ddb(cursor_ddb);
            cursor_ddb_ready_ = true;
            Log::info(
                "intel-display: DDB partitioned: plane1=[%u..%u] (%u blk), cursor=[%u..%u] (%u blk)",
                cfg.ddb.valid ? cfg.ddb.start : firmware_cfg.ddb.start,
                cfg.ddb.valid ? cfg.ddb.end : firmware_cfg.ddb.end,
                cfg.ddb.valid ? cfg.ddb.blocks() : firmware_cfg.ddb.blocks(),
                cursor_ddb.start,
                cursor_ddb.end,
                cursor_ddb.blocks()
            );
        } else {
            cursor_ddb_ready_ = false;
            Log::warning("intel-display: could not partition DDB for HW cursor, disabling HW cursor");
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

    CursorBuffer IntelDisplay::alloc_cursor(const CursorSize size) const {
        CursorBuffer out{};

        const u32 dim = static_cast<u32>(size);
        const usize surface_bytes = static_cast<usize>(dim) * dim * CURSOR_BYTES_PER_PIXEL;
        const usize surface_pages = (surface_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        const usize total_pages = surface_pages + CURSOR_PTE_PADDING_PAGES;
        constexpr u32 align_pages = CURSOR_SURFACE_ALIGN_BYTES / PAGE_SIZE;

        const core::GgttAllocation alloc = device_.ggtt().alloc_persistent_aligned(
            total_pages, align_pages, (1ULL << PtFlag::ReadWrite), CacheMode::WriteCombining, core::MOCS_UNCACHED
        );
        if (virt_null(alloc.cpu_addr)) {
            Log::error("intel-display: cursor: GGTT allocation failed (%zu pages)", total_pages);
            return out;
        }

        memset(virt_ptr(alloc.cpu_addr), 0, total_pages * PAGE_SIZE);

        out.alloc = alloc;
        out.capacity = size;
        out.surface_pages = static_cast<u32>(surface_pages);
        out.total_pages = static_cast<u32>(total_pages);
        out.valid = true;

        Log::info(
            "intel-display: cursor %ux%u gfx=0x%llx (%zu pages + %u PTE padding)",
            dim,
            dim,
            gfx_raw(alloc.gfx_addr),
            surface_pages,
            CURSOR_PTE_PADDING_PAGES
        );
        return out;
    }

    bool IntelDisplay::set_cursor_image(
        const u32* pixels, const u32 width, const u32 height, const i32 hotspot_x, const i32 hotspot_y
    ) {
        if (!scanout_ready_ || !pixels || width == 0 || height == 0 || width > 256 || height > 256) {
            return false;
        }

        kernel::MutexGuard guard(cursor_lock_);

        const CursorSize needed = pick_cursor_size(width, height);

        // Reuse existing persistent GGTT allocation if large enough.
        if (!cursor_buf_.valid || static_cast<u32>(needed) > static_cast<u32>(cursor_buf_.capacity)) {
            CursorBuffer buf = alloc_cursor(needed);
            if (!buf.valid) {
                return false;
            }
            cursor_buf_ = buf;
        }

        const u32 dim = static_cast<u32>(needed);
        auto* dst = static_cast<u32*>(virt_ptr(cursor_buf_.alloc.cpu_addr));
        memset(dst, 0, static_cast<usize>(dim) * dim * CURSOR_BYTES_PER_PIXEL);

        const usize row_bytes = static_cast<usize>(width) * CURSOR_BYTES_PER_PIXEL;
        for (u32 y = 0; y < height; ++y) {
            memcpy(dst + static_cast<usize>(y) * dim, pixels + static_cast<usize>(y) * width, row_bytes);
        }

        cursor_active_size_ = needed;
        cursor_hotspot_x_ = hotspot_x;
        cursor_hotspot_y_ = hotspot_y;

        sync_cursor_hw_locked(true);
        return true;
    }

    void IntelDisplay::move_cursor(const i32 x, const i32 y) {
        kernel::MutexGuard guard(cursor_lock_);
        cursor_x_ = x;
        cursor_y_ = y;
        sync_cursor_hw_locked(false);
    }

    void IntelDisplay::set_cursor_visible(const bool visible) {
        kernel::MutexGuard guard(cursor_lock_);
        if (cursor_visible_ == visible) {
            return;
        }
        cursor_visible_ = visible;
        sync_cursor_hw_locked(true);
    }

    void IntelDisplay::sync_cursor_hw_locked(const bool force_commit) {
        if (!scanout_ready_ || !cursor_buf_.valid || !cursor_visible_) {
            if (cursor_hw_active_) {
                primary_cursor().disable();
                cursor_hw_active_ = false;
            }
            return;
        }

        const i32 px = cursor_x_ - cursor_hotspot_x_;
        const i32 py = cursor_y_ - cursor_hotspot_y_;
        const i32 dim = static_cast<i32>(cursor_active_size_);
        const i32 sw = static_cast<i32>(screen_width_px());
        const i32 sh = static_cast<i32>(screen_height_px());

        // Hardware requires at least 1 cursor pixel over the pipe source area.
        const bool on_screen = (px + dim > 0) && (py + dim > 0) && (px < sw) && (py < sh);
        if (!on_screen) {
            if (cursor_hw_active_) {
                primary_cursor().disable();
                cursor_hw_active_ = false;
            }
            return;
        }

        if (!cursor_hw_active_ || force_commit) {
            const CursorConfig cfg = cursor_buf_.cursor_config(cursor_active_size_, px, py);
            if (primary_cursor().commit(cfg)) {
                cursor_hw_active_ = true;
            }
        } else {
            (void)primary_cursor().set_position(px, py);
        }
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

        if (primary_plane().flip_pending()) {
            kernel::chronos::checkpoint("disp.flip_wait", "begin");
            if (!primary_plane().wait_flip_done(FLIP_TIMEOUT_US)) {
                Log::warning("intel-display: flip did not complete within %u us", FLIP_TIMEOUT_US);
            }
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

        kernel::chronos::checkpoint("disp.present", "flip_queued");
    }

    int IntelDisplay::present_async() {
        if (!scanout_ready_) return -ENODEV;
        if (!curr_dirty_.valid) return 1;
        if (primary_plane().flip_pending()) return -EBUSY;

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

        if (!primary_plane().flip(scanout_[back_idx_].alloc.gfx_addr)) {
            Log::error("intel-display: present_async: flip rejected");
            return -EIO;
        }

        const u32 new_back = front_idx_;
        front_idx_ = back_idx_;
        back_idx_ = new_back;

        prev_dirty_ = curr_dirty_;
        curr_dirty_.reset();

        kernel::chronos::checkpoint("disp.present", "flip_queued");
        return 0;
    }

    int IntelDisplay::poll(PollTable* pt) {
        if (!scanout_ready_) return POLLHUP;
        if (pt) pt->add(primary_plane().flip_wait_queue());
        return primary_plane().flip_pending() ? 0 : POLLOUT;
    }
} // namespace gpu::intel::display
