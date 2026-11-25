// display_cursor.cpp
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

#include "display_cursor.h"

#include <vespera/log.h>

#include <gpu/intel/core/intel_gpu_device.h>

#include "display_mmio.h"

namespace gpu::intel::display {
    void DisplayCursor::init(core::IntelGpuDevice& device, const PipeId pipe_id) {
        device_ = &device;
        pipe_id_ = pipe_id;
        ddb_ = {};
        surface_ = {};
        mode_ = CUR_CTL::MODE_DISABLE;
        enabled_ = false;
        lock_.init("display-cursor");
    }

    void DisplayCursor::set_ddb(const DdbAllocation ddb) {
        SpinlockGuardIrq guard(lock_);
        ddb_ = ddb;
    }

    DdbAllocation DisplayCursor::ddb() const {
        SpinlockGuardIrq guard(lock_);
        return ddb_;
    }

    bool DisplayCursor::validate(const CursorConfig& cfg) {
        const u32 dim = cursor_mode_dimension(cfg.mode);
        if (dim == 0) {
            Log::error("intel-display: cursor config: invalid or disabled mode 0x%x", static_cast<u32>(cfg.mode));
            return false;
        }

        if (gfx_raw(cfg.surface) & (CURSOR_SURFACE_ALIGN_BYTES - 1)) {
            Log::error(
                "intel-display: cursor config: surface 0x%llx not %u KB aligned",
                gfx_raw(cfg.surface),
                CURSOR_SURFACE_ALIGN_BYTES >> 10
            );
            return false;
        }

        if (cfg.x < -CURSOR_POS_X_MAX || cfg.x > CURSOR_POS_X_MAX ||
            cfg.y < -CURSOR_POS_Y_MAX || cfg.y > CURSOR_POS_Y_MAX) {
            Log::error("intel-display: cursor config: position (%d, %d) out of range", cfg.x, cfg.y);
            return false;
        }

        if (cfg.rotate_180 && cursor_mode_is_2bpp(cfg.mode)) {
            Log::error("intel-display: cursor config: 180-deg rotation not valid for 2 bpp modes");
            return false;
        }

        if ((cfg.force_alpha_value != CUR_CTL::FORCE_ALPHA_DISABLE ||
                cfg.force_alpha_plane != CUR_CTL::FORCE_ALPHA_PLANE_DISABLE) &&
            !cursor_mode_is_argb(cfg.mode)) {
            Log::error("intel-display: cursor config: force alpha is only valid for ARGB modes");
            return false;
        }

        if (cfg.reduced_scan_lines != 0) {
            if (cursor_mode_is_2bpp(cfg.mode) || cfg.rotate_180) {
                Log::error(
                    "intel-display: cursor config: FBC size reduction not allowed with 2 bpp or 180-deg rotation");
                return false;
            }
            if (cfg.reduced_scan_lines < 8 || cfg.reduced_scan_lines > dim) {
                Log::error(
                    "intel-display: cursor config: invalid reduced_scan_lines %u (valid: 8..%u)",
                    cfg.reduced_scan_lines,
                    dim
                );
                return false;
            }
        }

        return true;
    }

    void DisplayCursor::arm_base_locked(const gfx_addr_t surface) const {
        CUR_BASE base{};
        base.raw = 0;
        base.set_address(static_cast<u32>(gfx_raw(surface)));

        // Flush any Write-Combining pixel writes before arming the hardware.
        asm volatile("mfence" ::: "memory");
        mmio_write(*device_, reg(CUR_BASE_A), base);
        asm volatile("mfence" ::: "memory");
    }

    bool DisplayCursor::commit(const CursorConfig& cfg) {
        if (!validate(cfg)) {
            return false;
        }

        CUR_CTL ctl{};
        ctl.raw = 0;
        ctl.mode = cfg.mode;
        ctl.force_alpha_value = cfg.force_alpha_value;
        ctl.force_alpha_plane = cfg.force_alpha_plane;
        ctl.trickle_feed_enable = 0; // 0 = enabled (do not set to 1)
        ctl.rotate_180 = cfg.rotate_180 ? 1u : 0u;
        ctl.pipe_csc_enable = cfg.pipe_csc_enable ? 1u : 0u;
        ctl.gamma_enable = cfg.gamma_enable ? 1u : 0u;

        CUR_POS pos{};
        pos.raw = 0;
        pos.set(cfg.x, cfg.y);

        CUR_FBC_CTL fbc{};
        fbc.raw = 0;
        if (cfg.reduced_scan_lines != 0) {
            fbc.size_reduction_enable = 1;
            fbc.set_scan_lines(cfg.reduced_scan_lines);
        }

        const u32 dim = cursor_mode_dimension(cfg.mode);

        SpinlockGuardIrq guard(lock_);

        const DdbAllocation active_ddb = cfg.ddb.valid ? cfg.ddb : ddb_;
        if (!active_ddb.valid || active_ddb.blocks() < 8) {
            Log::error(
                "intel-display: pipe %u cursor commit rejected: no valid DDB allocation (%u blocks)",
                static_cast<u32>(pipe_id_),
                active_ddb.blocks()
            );
            return false;
        }

        // Program DDB allocation for cursor plane (double-buffered, armed by CUR_BASE).
        PLANE_BUF_CFG buf_cfg{};
        buf_cfg.raw = 0;
        buf_cfg.set(active_ddb.start, active_ddb.end);
        mmio_write(*device_, reg(CUR_BUF_CFG_A), buf_cfg);

        // Program level-0 cursor watermark (lines ignored by HW for level 0) and
        // disable levels 1..7 + transition WM to avoid LP-state FIFO underruns.
        u32 wm0_blocks = (dim <= 64) ? 4u : (dim <= 128) ? 8u : 16u;
        if (wm0_blocks >= active_ddb.blocks()) {
            wm0_blocks = active_ddb.blocks() - 1;
        }
        PLANE_WM wm0{};
        wm0.raw = 0;
        wm0.set(wm0_blocks, 0, true);
        mmio_write(*device_, plane_wm_addr(reg(CUR_WM_A), 0), wm0);

        PLANE_WM wm_off{};
        wm_off.raw = 0;
        for (u32 lvl = 1; lvl < PLANE_WM_LEVEL_COUNT; ++lvl) {
            mmio_write(*device_, plane_wm_addr(reg(CUR_WM_A), lvl), wm_off);
        }
        mmio_write(*device_, reg(CUR_WM_TRANS_A), wm_off);

        // Double-buffered registers — armed together when CUR_BASE is written last.
        mmio_write(*device_, reg(CUR_FBC_CTL_A), fbc);
        mmio_write(*device_, reg(CUR_POS_A), pos);
        mmio_write(*device_, reg(CUR_CTL_A), ctl);

        surface_ = cfg.surface;
        mode_ = cfg.mode;
        enabled_ = true;

        arm_base_locked(cfg.surface);

        Log::log_dbc(
            "intel-display: pipe %u cursor commit surf=0x%llx mode=0x%x pos=(%d,%d) ddb=[%u..%u] wm0=%u",
            static_cast<u32>(pipe_id_),
            gfx_raw(cfg.surface),
            static_cast<u32>(cfg.mode),
            cfg.x,
            cfg.y,
            active_ddb.start,
            active_ddb.end,
            wm0_blocks
        );
        return true;
    }

    bool DisplayCursor::set_surface(const gfx_addr_t surface) {
        if (gfx_raw(surface) & (CURSOR_SURFACE_ALIGN_BYTES - 1)) {
            Log::error(
                "intel-display: cursor set_surface: 0x%llx not %u KB aligned",
                gfx_raw(surface),
                CURSOR_SURFACE_ALIGN_BYTES >> 10
            );
            return false;
        }

        SpinlockGuardIrq guard(lock_);
        surface_ = surface;
        if (enabled_) {
            arm_base_locked(surface_);
        }
        return true;
    }

    bool DisplayCursor::set_position(const i32 x, const i32 y) const {
        if (x < -CURSOR_POS_X_MAX || x > CURSOR_POS_X_MAX ||
            y < -CURSOR_POS_Y_MAX || y > CURSOR_POS_Y_MAX) {
            return false;
        }

        CUR_POS pos{};
        pos.raw = 0;
        pos.set(x, y);

        SpinlockGuardIrq guard(lock_);
        mmio_write(*device_, reg(CUR_POS_A), pos);

        // CUR_POS is double-buffered and only latches once CUR_BASE is written.
        if (enabled_) {
            arm_base_locked(surface_);
        }
        return true;
    }

    void DisplayCursor::set_palette(const CUR_PAL entries[CURSOR_PAL_COUNT]) const {
        SpinlockGuardIrq guard(lock_);
        for (u32 i = 0; i < CURSOR_PAL_COUNT; ++i) {
            mmio_write(*device_, cursor_pal_reg(pipe_id_, i), entries[i]);
        }
        if (enabled_) {
            arm_base_locked(surface_);
        }
    }

    void DisplayCursor::disable() {
        CUR_CTL ctl{};
        ctl.raw = 0;
        ctl.mode = CUR_CTL::MODE_DISABLE;

        SpinlockGuardIrq guard(lock_);
        mmio_write(*device_, reg(CUR_CTL_A), ctl);

        PLANE_WM wm_off{};
        wm_off.raw = 0;
        for (u32 lvl = 0; lvl < PLANE_WM_LEVEL_COUNT; ++lvl) {
            mmio_write(*device_, plane_wm_addr(reg(CUR_WM_A), lvl), wm_off);
        }
        mmio_write(*device_, reg(CUR_WM_TRANS_A), wm_off);

        PLANE_BUF_CFG buf_off{};
        buf_off.raw = 0;
        mmio_write(*device_, reg(CUR_BUF_CFG_A), buf_off);

        enabled_ = false;
        mode_ = CUR_CTL::MODE_DISABLE;

        // Arm the disable + empty DDB/WMs via CUR_BASE.
        arm_base_locked(make_gfx(0));
    }

    bool DisplayCursor::read_config(CursorConfig& out) const {
        const auto ctl = mmio_read<CUR_CTL>(*device_, reg(CUR_CTL_A));
        if (!ctl.enabled()) {
            return false;
        }

        const auto base = mmio_read<CUR_BASE>(*device_, reg(CUR_BASE_A));
        const auto pos = mmio_read<CUR_POS>(*device_, reg(CUR_POS_A));
        const auto fbc = mmio_read<CUR_FBC_CTL>(*device_, reg(CUR_FBC_CTL_A));
        const auto buf_cfg = mmio_read<PLANE_BUF_CFG>(*device_, reg(CUR_BUF_CFG_A));

        out.surface = make_gfx(base.address());
        out.mode = ctl.mode;
        out.x = pos.x();
        out.y = pos.y();
        out.rotate_180 = ctl.rotate_180 != 0;
        out.gamma_enable = ctl.gamma_enable != 0;
        out.pipe_csc_enable = ctl.pipe_csc_enable != 0;
        out.force_alpha_value = ctl.force_alpha_value;
        out.force_alpha_plane = ctl.force_alpha_plane;
        out.reduced_scan_lines = fbc.size_reduction_enable ? fbc.scan_lines() : 0;
        if (buf_cfg.raw != 0 && buf_cfg.buffer_end >= buf_cfg.buffer_start) {
            out.ddb = {static_cast<u16>(buf_cfg.buffer_start), static_cast<u16>(buf_cfg.buffer_end), true};
        } else {
            out.ddb = {};
        }
        return true;
    }

    gfx_addr_t DisplayCursor::live_surface() const {
        const auto live = mmio_read<CUR_SURFLIVE>(*device_, reg(CUR_SURFLIVE_A));
        return make_gfx(live.address());
    }

    bool DisplayCursor::enabled() const {
        SpinlockGuardIrq guard(lock_);
        return enabled_;
    }
} // namespace gpu::intel::display
