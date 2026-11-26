// intel_display.h
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

#ifndef VESPERAOS_INTEL_DISPLAY_H
#define VESPERAOS_INTEL_DISPLAY_H

#include <vespera/graphics/display_types.h>
#include <vespera/mm/addr.h>
#include <vespera/sync/mutex.h>
#include <vespera/types.h>

#include <gpu/intel/core/ggtt_allocator.h>
#include <gpu/intel/regs/interrupt_regs.h>

#include "display_pipe.h"
#include "display_plane.h"
#include "display_surface.h"
#include "vespera/graphics/IRenderDriver.h"

namespace gpu::intel::core {
    class IntelGpuDevice;
}

namespace gpu::intel::display {
    struct DirtyRect {
        u32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        bool valid = false;

        void reset() { valid = false; }

        void expand(const u32 x, const u32 y, const u32 w, const u32 h) {
            if (w == 0 || h == 0) return;
            const u32 nx1 = x + w;
            const u32 ny1 = y + h;
            if (!valid) {
                x0 = x;
                y0 = y;
                x1 = nx1;
                y1 = ny1;
                valid = true;
                return;
            }
            if (x < x0) x0 = x;
            if (y < y0) y0 = y;
            if (nx1 > x1) x1 = nx1;
            if (ny1 > y1) y1 = ny1;
        }

        [[nodiscard]] u64 area() const {
            return valid ? static_cast<u64>(x1 - x0) * (y1 - y0) : 0;
        }

        [[nodiscard]] DirtyRect union_with(const DirtyRect& o) const {
            if (!valid) return o;
            if (!o.valid) return *this;
            DirtyRect r{};
            r.x0 = (x0 < o.x0) ? x0 : o.x0;
            r.y0 = (y0 < o.y0) ? y0 : o.y0;
            r.x1 = (x1 > o.x1) ? x1 : o.x1;
            r.y1 = (y1 > o.y1) ? y1 : o.y1;
            r.valid = true;
            return r;
        }
    };

    /// A GGTT-backed scanout surface with all the alignment / padding rules
    /// PLANE_SURF imposes already applied (see IntelDisplay::alloc_scanout()).
    struct ScanoutBuffer {
        core::GgttAllocation alloc{};
        u32 width = 0;
        u32 height = 0;
        u32 pitch = 0;
        TileMode tiling = TileMode::Linear;
        u32 surface_pages = 0; ///< pages covered by pitch * height
        u32 total_pages = 0;   ///< surface_pages + SCANOUT_PTE_PADDING_PAGES
        bool valid = false;

        [[nodiscard]] PlaneConfig plane_config() const {
            PlaneConfig cfg{};
            cfg.surface = alloc.gfx_addr;
            cfg.width = width;
            cfg.height = height;
            cfg.pitch_bytes = pitch;
            cfg.tiling = tiling;
            return cfg;
        }
    };

    struct CursorBuffer {
        core::GgttAllocation alloc{};
        CursorSize capacity = CursorSize::Size64;
        u32 surface_pages = 0;
        u32 total_pages = 0;
        bool valid = false;

        [[nodiscard]] CursorConfig cursor_config(const CursorSize mode_size, const i32 x, const i32 y) const {
            CursorConfig cfg{};
            cfg.surface = alloc.gfx_addr;
            cfg.mode = to_argb_cursor_mode(mode_size);
            cfg.x = x;
            cfg.y = y;
            return cfg;
        }
    };

    /**
     * @brief Display engine front-end of the Intel GPU.
     *
     * Peer of IntelBcs / IntelRcs: borrows the same IntelGpuDevice (MMIO,
     * GGTT, IRQ vector) but is not a command streamer. Today it adopts the
     * mode the firmware (UEFI GOP) left behind and drives the planes; real
     * modeset (transcoder / DDI / PLL / panel power) will live next to it in
     * this directory.
     *
     * Lifetime: constructed by IntelBltPciDriver after IntelGpuDevice::init()
     * and before any engine that wants a plane; init() registers it with the
     * device's shared IRQ dispatcher.
     */
    class IntelDisplay final : public IRenderDriver {
    public:
        explicit IntelDisplay(core::IntelGpuDevice& device);

        IntelDisplay(const IntelDisplay&) = delete;
        IntelDisplay& operator=(const IntelDisplay&) = delete;

        [[nodiscard]] bool init();

        [[nodiscard]] DisplayPipe& pipe(const PipeId id) {
            return pipes_[static_cast<u32>(id)];
        }

        [[nodiscard]] DisplayPlane& plane(const PipeId pipe_id, const PlaneId plane_id) {
            return pipe(pipe_id).plane(plane_id);
        }

        /// Pipe A, plane 1: the primary plane the firmware scans out on the
        /// T480's eDP panel.
        [[nodiscard]] DisplayPlane& primary_plane() {
            return plane(PipeId::A, PlaneId::Plane1);
        }

        [[nodiscard]] DisplayCursor& cursor(const PipeId pipe_id) {
            return pipe(pipe_id).cursor();
        }

        [[nodiscard]] DisplayCursor& primary_cursor() {
            return cursor(PipeId::A);
        }

        /// Allocates a zeroed linear ARGB8888 cursor surface in the GGTT
        [[nodiscard]] CursorBuffer alloc_cursor(CursorSize size) const;

        /// Uploads an ARGB8888 (0xAARRGGBB, pre-multiplied alpha) cursor image
        /// up to 256x256 px and configures its hotspot. Reuses the existing GGTT
        /// buffer whenever the image fits.
        [[nodiscard]] bool set_cursor_image(
            const u32* pixels, u32 width, u32 height, i32 hotspot_x, i32 hotspot_y
        ) override;

        /// Moves the cursor hotspot to screen coordinates (x, y).
        void move_cursor(i32 x, i32 y) override;

        void set_cursor_visible(bool visible) override;

        [[nodiscard]] bool cursor_visible() const {
            kernel::MutexGuard guard(cursor_lock_);
            return cursor_visible_;
        }

        [[nodiscard]] bool has_hw_cursor() const override {
            return scanout_ready_ && cursor_ddb_ready_;
        }

        /// Allocates a zeroed XRGB8888 scanout surface of `res` in the GGTT:
        /// pitch and height rounded up for the tiling, base aligned to
        /// 256 KB (Linear/X) or 1 MB (Y), plus the 136 padding PTEs.
        /// `valid` is false on failure.
        [[nodiscard]] ScanoutBuffer alloc_scanout(Resolution res, TileMode tiling) const;

        /// Called from IntelGpuDevice::device_irq_handler() with the
        /// MASTER_INT_CTL value it just read. Returns true if a pipe handled
        /// something.
        [[nodiscard]] bool handle_irq(const MASTER_INT_CTL& master);

        [[nodiscard]] bool fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) override;
        [[nodiscard]] bool blit_region(
            const u32* pixels, u32 src_stride, u32 src_x, u32 src_y, u32 w, u32 h, u32 dst_x, u32 dst_y
        ) override;
        void present() override;

        int present_async() override;
        int poll(PollTable* pt) override;

        [[nodiscard]] u32 screen_width_px() const override {
            return scanout_ready_ ? scanout_[front_idx_].width : 0;
        }

        [[nodiscard]] u32 screen_height_px() const override {
            return scanout_ready_ ? scanout_[front_idx_].height : 0;
        }

        [[nodiscard]] u32 bytes_per_scanline() const override {
            return scanout_ready_ ? scanout_[front_idx_].pitch : 0;
        }

    private:
        void log_firmware_state();
        bool init_scanout();
        void upload_rect_to_back(const DirtyRect& r) const;
        void sync_cursor_hw_locked(bool force_commit);

        core::IntelGpuDevice& device_;
        DisplayPipe pipes_[PIPE_COUNT];

        mutable kernel::Mutex cursor_lock_;
        CursorBuffer cursor_buf_{};
        CursorSize cursor_active_size_ = CursorSize::Size64;
        i32 cursor_x_{0};
        i32 cursor_y_{0};
        i32 cursor_hotspot_x_{0};
        i32 cursor_hotspot_y_{0};
        bool cursor_visible_{false};
        bool cursor_hw_active_{false};
        bool cursor_ddb_ready_{false};

        ScanoutBuffer scanout_[2]{};
        u32 front_idx_{0u};
        u32 back_idx_{1u};
        bool scanout_ready_{false};

        u32* shadow_ = nullptr;
        u32 shadow_stride_px_{0u};
        DirtyRect curr_dirty_{};
        DirtyRect prev_dirty_{};
    };
} // namespace gpu::intel::display

#endif  // VESPERAOS_INTEL_DISPLAY_H
