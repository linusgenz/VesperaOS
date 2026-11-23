// display_plane.h
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

#ifndef VESPERAOS_DISPLAY_PLANE_H
#define VESPERAOS_DISPLAY_PLANE_H

#include <vespera/mm/addr.h>
#include <vespera/sync/wait_queue.h>
#include <vespera/types.h>

#include "display_reg_addr.h"
#include "display_surface.h"

namespace gpu::intel::core {
    class IntelGpuDevice;
}

namespace gpu::intel::display {

    class DisplayPipe;

    /**
     * @brief One universal display plane (PLANE_CTL/STRIDE/SIZE/POS/OFFSET/SURF).
     *
     * Independent of every command streamer: all programming is plain MMIO.
     * A write to PLANE_SURF arms the double-buffered registers, so commit()
     * and flip() always write it last.
     *
     * Flip completion is tracked per plane (flip_pending()) and signalled
     * from DisplayPipe's interrupt handler.
     *
     * Two-phase init (init()) because planes live in fixed arrays inside
     * DisplayPipe.
     */
    class DisplayPlane {
    public:
        DisplayPlane() = default;

        DisplayPlane(const DisplayPlane&) = delete;
        DisplayPlane& operator=(const DisplayPlane&) = delete;

        void init(core::IntelGpuDevice& device, DisplayPipe& pipe, PipeId pipe_id, PlaneId plane_id);

        /// Validates `cfg`, programs STRIDE/SIZE/POS/OFFSET/CTL (plane enabled)
        /// and finally SURF, which arms the flip. Returns false (and writes
        /// nothing) if the config violates a hardware constraint.
        [[nodiscard]] bool commit(const PlaneConfig& cfg);

        /// Flips to another surface with the geometry of the last commit().
        /// Writes only PLANE_SURF. Returns false if `surface` violates the
        /// alignment for the committed tiling.
        [[nodiscard]] bool flip(gfx_addr_t surface);

        /// Clears PLANE_CTL.plane_enable and arms it via PLANE_SURF.
        void disable();

        /// Reads the current hardware state (e.g. what the firmware / GOP set
        /// up). Returns false if the plane is disabled or uses a tiling /
        /// format this driver does not model; `out` is then incomplete.
        [[nodiscard]] bool read_config(PlaneConfig& out) const;

        [[nodiscard]] bool flip_pending() const;

        /// Polls (sleeping) until the last flip completed or `timeout_us`
        /// passed. Returns true if no flip is pending any more.
        [[nodiscard]] bool wait_flip_done(u32 timeout_us) const;

        [[nodiscard]] PipeId pipe_id() const {
            return pipe_id_;
        }
        [[nodiscard]] PlaneId plane_id() const {
            return plane_id_;
        }

    private:
        friend class DisplayPipe;

        /// Called from DisplayPipe::handle_irq() with the pipe lock held.
        void on_flip_complete();

        /// Marks the flip pending, arms the pipe's vblank interrupt and writes
        /// PLANE_SURF. The caller holds the pipe lock.
        void write_surf_and_arm(gfx_addr_t surface);

        [[nodiscard]] bool validate(const PlaneConfig& cfg) const;

        [[nodiscard]] u32 reg(const u32 reg_1a) const {
            return plane_reg(pipe_id_, plane_id_, reg_1a);
        }

        core::IntelGpuDevice* device_ = nullptr;
        DisplayPipe* pipe_ = nullptr;
        PipeId pipe_id_ = PipeId::A;
        PlaneId plane_id_ = PlaneId::Plane1;

        TileMode tiling_ = TileMode::Linear;  ///< of the last commit(), for flip() checks
        u32 flip_pending_ = 0;                ///< accessed with __atomic builtins
        mutable WaitQueue flip_waiters_;      ///< woken by on_flip_complete() in VBlank IRQ
    };

}  // namespace gpu::intel::display

#endif  // VESPERAOS_DISPLAY_PLANE_H
