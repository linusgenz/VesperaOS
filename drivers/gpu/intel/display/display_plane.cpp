// display_plane.cpp
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

#include "display_plane.h"

#include <vespera/log.h>
#include <vespera/scheduling.h>
#include <vespera/time.h>

#include <gpu/intel/core/intel_gpu_device.h>
#include "../kernel/units/unit.h"

#include "display_mmio.h"
#include "display_pipe.h"

namespace gpu::intel::display {

    void DisplayPlane::init(
        core::IntelGpuDevice& device, DisplayPipe& pipe, const PipeId pipe_id, const PlaneId plane_id
    ) {
        device_ = &device;
        pipe_ = &pipe;
        pipe_id_ = pipe_id;
        plane_id_ = plane_id;
        tiling_ = TileMode::Linear;
        __atomic_store_n(&flip_pending_, 0u, __ATOMIC_RELEASE);
    }

    // =========================================================================
    // Validation
    // =========================================================================

    bool DisplayPlane::validate(const PlaneConfig& cfg) const {
        if (cfg.width == 0 || cfg.height == 0 || cfg.width > PLANE_MAX_WIDTH || cfg.height > PLANE_MAX_HEIGHT) {
            Log::error("intel-display: plane config: invalid size %ux%u", cfg.width, cfg.height);
            return false;
        }

        const u32 pitch_unit = pitch_alignment_bytes(cfg.tiling);
        if (cfg.pitch_bytes == 0 || cfg.pitch_bytes % pitch_unit != 0 || cfg.pitch_bytes / pitch_unit > 1023) {
            Log::error(
                "intel-display: plane config: pitch %u not a valid multiple of %u (10-bit stride)",
                cfg.pitch_bytes,
                pitch_unit
            );
            return false;
        }

        const u32 align = surface_alignment_bytes(cfg.tiling);
        if (gfx_raw(cfg.surface) & (align - 1)) {
            Log::error(
                "intel-display: plane config: surface 0x%llx not %u KB aligned", gfx_raw(cfg.surface), align >> 10
            );
            return false;
        }

        // 90/270 need Y-tiling plus the x/y offset fixup described in
        // PLANE_OFFSET; not implemented yet.
        if (cfg.rotation != PLANE_CTL::ROTATE_0 && cfg.rotation != PLANE_CTL::ROTATE_180) {
            Log::error("intel-display: plane config: rotation %u not supported yet", static_cast<u32>(cfg.rotation));
            return false;
        }

        return true;
    }

    // =========================================================================
    // Programming
    // =========================================================================

    void DisplayPlane::write_surf_and_arm(const gfx_addr_t surface) {
        PLANE_SURF surf{};
        surf.raw = 0;
        surf.set_address(static_cast<u32>(gfx_raw(surface)));

        // The flag is set before the SURF write so the interrupt can never
        // observe a completed flip that is not marked pending yet; the pipe
        // lock (held by the caller) keeps the handler out until we are done.
        __atomic_store_n(&flip_pending_, 1u, __ATOMIC_RELEASE);
        pipe_->vblank_arm_locked();

        asm volatile("mfence" ::: "memory");
        mmio_write(*device_, reg(PLANE_SURF_1_A), surf);
        asm volatile("mfence" ::: "memory");
    }

    bool DisplayPlane::commit(const PlaneConfig& cfg) {
        if (!validate(cfg)) {
            return false;
        }

        PLANE_CTL ctl{};
        ctl.raw = 0;
        ctl.source_pixel_format = cfg.format;
        ctl.rgb_color_order = 0;  // BGRX
        ctl.tiled_surface = to_plane_tiling(cfg.tiling);
        ctl.plane_rotation = cfg.rotation;
        ctl.plane_enable = 1;

        PLANE_STRIDE stride{};
        stride.raw = 0;
        stride.set_stride_bytes(cfg.pitch_bytes, ctl.tiled_surface);

        PLANE_SIZE size{};
        size.raw = 0;
        size.set(cfg.width, cfg.height);

        PLANE_POS pos{};
        pos.raw = 0;
        pos.set(cfg.dst_x, cfg.dst_y);

        PLANE_OFFSET offset{};
        offset.raw = 0;
        offset.set(cfg.src_x, cfg.src_y);

        SpinlockGuardIrq guard(pipe_->lock());

        // All of these are double-buffered and only latch once PLANE_SURF is
        // written (see display_regs.h), so the enable bit does not need a
        // separate write.
        mmio_write(*device_, reg(PLANE_STRIDE_1_A), stride);
        mmio_write(*device_, reg(PLANE_SIZE_1_A), size);
        mmio_write(*device_, reg(PLANE_POS_1_A), pos);
        mmio_write(*device_, reg(PLANE_OFFSET_1_A), offset);
        mmio_write(*device_, reg(PLANE_CTL_1_A), ctl);

        tiling_ = cfg.tiling;
        write_surf_and_arm(cfg.surface);

        Log::log_dbc(
            "intel-display: pipe %u plane %u commit surf=0x%llx %ux%u pitch=%u tiling=%u",
            static_cast<u32>(pipe_id_),
            static_cast<u32>(plane_id_) + 1,
            gfx_raw(cfg.surface),
            cfg.width,
            cfg.height,
            cfg.pitch_bytes,
            static_cast<u32>(cfg.tiling)
        );
        return true;
    }

    bool DisplayPlane::flip(const gfx_addr_t surface) {
        const u32 align = surface_alignment_bytes(tiling_);
        if (gfx_raw(surface) & (align - 1)) {
            Log::error("intel-display: flip: surface 0x%llx not %u KB aligned", gfx_raw(surface), align >> 10);
            return false;
        }

        SpinlockGuardIrq guard(pipe_->lock());
        write_surf_and_arm(surface);
        return true;
    }

    void DisplayPlane::disable() {
        SpinlockGuardIrq guard(pipe_->lock());

        PLANE_CTL ctl = mmio_read<PLANE_CTL>(*device_, reg(PLANE_CTL_1_A));
        ctl.plane_enable = 0;
        mmio_write(*device_, reg(PLANE_CTL_1_A), ctl);

        // The disable only latches with a PLANE_SURF write; rewrite the
        // current address.
        const PLANE_SURF cur = mmio_read<PLANE_SURF>(*device_, reg(PLANE_SURF_1_A));
        write_surf_and_arm(make_gfx(cur.address()));
    }

    // =========================================================================
    // Readback / state
    // =========================================================================

    bool DisplayPlane::read_config(PlaneConfig& out) const {
        const PLANE_CTL ctl = mmio_read<PLANE_CTL>(*device_, reg(PLANE_CTL_1_A));
        if (!ctl.plane_enable) {
            return false;
        }

        switch (ctl.tiled_surface) {
            case PLANE_CTL::TILING_LINEAR:
                out.tiling = TileMode::Linear;
                break;
            case PLANE_CTL::TILING_X:
                out.tiling = TileMode::X;
                break;
            case PLANE_CTL::TILING_Y:
                out.tiling = TileMode::Y;
                break;
            default:
                return false;
        }

        const auto stride = mmio_read<PLANE_STRIDE>(*device_, reg(PLANE_STRIDE_1_A));
        const auto size = mmio_read<PLANE_SIZE>(*device_, reg(PLANE_SIZE_1_A));
        const auto pos = mmio_read<PLANE_POS>(*device_, reg(PLANE_POS_1_A));
        const auto offset = mmio_read<PLANE_OFFSET>(*device_, reg(PLANE_OFFSET_1_A));
        const auto surf = mmio_read<PLANE_SURF>(*device_, reg(PLANE_SURF_1_A));

        out.surface = make_gfx(surf.address());
        out.width = size.width_px();
        out.height = size.height_px();
        out.src_x = offset.x;
        out.src_y = offset.y;
        out.dst_x = pos.x;
        out.dst_y = pos.y;
        out.pitch_bytes = stride.stride_bytes(ctl.tiled_surface);
        out.format = ctl.source_pixel_format;
        out.rotation = ctl.plane_rotation;
        return true;
    }

    bool DisplayPlane::flip_pending() const {
        return __atomic_load_n(&flip_pending_, __ATOMIC_ACQUIRE) != 0;
    }

    bool DisplayPlane::wait_flip_done(const u32 timeout_us) const {
        if (!flip_pending()) {
            return true;
        }

        const u64 deadline_ns = kernel::time::get_uptime_ns() + static_cast<u64>(timeout_us) * 1'000ULL;

        Unit* cur = kernel::scheduling::get_current_unit();
        if (!cur || cur->is_idle) {
            while (flip_pending()) {
                if (kernel::time::get_uptime_ns() >= deadline_ns) {
                    return false;
                }
                kernel::time::sleep_us(50);
            }
            return true;
        }

        const u8 cpu_id = cur->cpu_id;

        auto cleanup = [&]() {
            flip_waiters_.remove(cur);
            kernel::scheduling::remove_blocked_unit(cur);
            cur->sleep_context.wakeup_ns = 0;
            kernel::scheduling::remove_unit(cur);
            cur->state = UnitState::Running;
        };

        while (flip_pending()) {
            if (kernel::time::get_uptime_ns() >= deadline_ns) {
                return false;
            }

            cur->sleep_context.wakeup_ns = deadline_ns;
            kernel::time::sleep_timer::notify_sleep(cpu_id, deadline_ns);
            kernel::scheduling::add_blocked_unit(cur, cpu_id);
            flip_waiters_.add_wait(cur);

            if (!flip_pending()) {
                cleanup();
                return true;
            }

            if (static_cast<UnitState>(
                    __atomic_load_n(reinterpret_cast<const u8*>(&cur->state), __ATOMIC_ACQUIRE)
                ) == UnitState::Blocked) {
                kernel::scheduling::yield();
            }

            cleanup();
        }
        return true;
    }

    void DisplayPlane::on_flip_complete() {
        __atomic_store_n(&flip_pending_, 0u, __ATOMIC_RELEASE);
        flip_waiters_.wake_all_irq();
    }

}  // namespace gpu::intel::display
