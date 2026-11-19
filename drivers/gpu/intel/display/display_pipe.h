// display_pipe.h
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

#ifndef VESPERAOS_DISPLAY_PIPE_H
#define VESPERAOS_DISPLAY_PIPE_H

#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

#include "display_plane.h"
#include "display_surface.h"

namespace gpu::intel::core {
    class IntelGpuDevice;
}

namespace gpu::intel::display {

    /**
     * @brief One display pipe (A/B/C): owns its planes and its DE_PIPE_*
     *        interrupt registers.
     *
     * Interrupt model (carried over from the old IntelBcs/IntelGpuDevice
     * DE-Pipe-A code):
     *  - plane flip-done interrupts are unmasked permanently,
     *  - vblank is masked and only armed one-shot while a flip is pending,
     *    as a fallback completion signal; it is disarmed again as soon as no
     *    plane on this pipe has a flip pending.
     *
     * lock_ serializes the IMR/IER read-modify-writes and the planes'
     * flip_pending_ bookkeeping between process context (flip()/commit())
     * and the interrupt handler.
     */
    class DisplayPipe {
    public:
        DisplayPipe() = default;

        DisplayPipe(const DisplayPipe&) = delete;
        DisplayPipe& operator=(const DisplayPipe&) = delete;

        /// Initializes the pipe and its planes. Does not touch hardware.
        void init(core::IntelGpuDevice& device, PipeId id);

        /// Clears stale IIR bits and enables the flip-done interrupts.
        /// Call after the device's IRQ vector / master enable is set up.
        void irq_init();

        /// Called from IntelDisplay::handle_irq() (interrupt context) when
        /// MASTER_INT_CTL reports this pipe pending. Returns true if it
        /// handled something.
        [[nodiscard]] bool handle_irq();

        [[nodiscard]] DisplayPlane& plane(const PlaneId id) {
            return planes_[static_cast<u32>(id)];
        }

        [[nodiscard]] PipeId id() const {
            return id_;
        }

    private:
        friend class DisplayPlane;

        [[nodiscard]] Spinlock& lock() {
            return lock_;
        }

        /// The caller holds lock_.
        void vblank_arm_locked();
        void vblank_disarm_locked();
        [[nodiscard]] bool any_flip_pending() const;

        core::IntelGpuDevice* device_ = nullptr;
        PipeId id_ = PipeId::A;

        DisplayPlane planes_[PLANES_PER_PIPE];

        Spinlock lock_;
        bool irq_enabled_ = false;
    };

}  // namespace gpu::intel::display

#endif  // VESPERAOS_DISPLAY_PIPE_H
