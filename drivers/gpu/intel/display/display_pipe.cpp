// display_pipe.cpp
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

#include "display_pipe.h"

#include <vespera/log.h>

#include <gpu/intel/core/intel_gpu_device.h>

#include "display_mmio.h"
#include "display_reg_addr.h"

namespace gpu::intel::display {

    void DisplayPipe::init(core::IntelGpuDevice& device, const PipeId id) {
        device_ = &device;
        id_ = id;
        irq_enabled_ = false;

        lock_.init("display-pipe");

        for (u32 i = 0; i < PLANES_PER_PIPE; i++) {
            planes_[i].init(device, *this, id, static_cast<PlaneId>(i));
        }

        cursor_.init(device, id);
    }

    void DisplayPipe::irq_init() {
        const u32 iir_reg = de_pipe_reg(id_, DE_PIPE_A_IIR);
        const u32 imr_reg = de_pipe_reg(id_, DE_PIPE_A_IMR);
        const u32 ier_reg = de_pipe_reg(id_, DE_PIPE_A_IER);

        SpinlockGuardIrq guard(lock_);

        // Clear anything stale before enabling (IIR is write-1-to-clear).
        mmio_write(*device_, iir_reg, DE_PIPE_VBLANK_MASK | DE_PIPE_FLIP_DONE_ALL_MASK);

        // 1 = masked. vblank stays masked until a flip arms it.
        u32 imr = mmio_read(*device_, imr_reg);
        imr |= DE_PIPE_VBLANK_MASK;
        imr &= ~DE_PIPE_FLIP_DONE_ALL_MASK;
        mmio_write(*device_, imr_reg, imr);

        u32 ier = mmio_read(*device_, ier_reg);
        ier &= ~DE_PIPE_VBLANK_MASK;
        ier |= DE_PIPE_FLIP_DONE_ALL_MASK;
        mmio_write(*device_, ier_reg, ier);
        mmio_post(*device_, ier_reg);

        irq_enabled_ = true;
    }

    void DisplayPipe::vblank_arm_locked() const {
        if (!irq_enabled_) {
            return;
        }

        const u32 imr_reg = de_pipe_reg(id_, DE_PIPE_A_IMR);
        const u32 ier_reg = de_pipe_reg(id_, DE_PIPE_A_IER);

        mmio_write(*device_, imr_reg, mmio_read(*device_, imr_reg) & ~DE_PIPE_VBLANK_MASK);  // unmask
        mmio_write(*device_, ier_reg, mmio_read(*device_, ier_reg) | DE_PIPE_VBLANK_MASK);   // enable
        mmio_post(*device_, ier_reg);
    }

    void DisplayPipe::vblank_disarm_locked() const {
        const u32 imr_reg = de_pipe_reg(id_, DE_PIPE_A_IMR);
        const u32 ier_reg = de_pipe_reg(id_, DE_PIPE_A_IER);

        mmio_write(*device_, imr_reg, mmio_read(*device_, imr_reg) | DE_PIPE_VBLANK_MASK);   // mask
        mmio_write(*device_, ier_reg, mmio_read(*device_, ier_reg) & ~DE_PIPE_VBLANK_MASK);  // disable
        mmio_post(*device_, ier_reg);
    }

    bool DisplayPipe::any_flip_pending() const {
        for (u32 i = 0; i < PLANES_PER_PIPE; i++) {
            if (planes_[i].flip_pending()) {
                return true;
            }
        }
        return false;
    }

    bool DisplayPipe::handle_irq() {
        if (!irq_enabled_) {
            return false;
        }

        const u32 iir_reg = de_pipe_reg(id_, DE_PIPE_A_IIR);

        SpinlockGuardIrq guard(lock_);

        const u32 pending = mmio_read(*device_, iir_reg) & (DE_PIPE_VBLANK_MASK | DE_PIPE_FLIP_DONE_ALL_MASK);
        if (pending == 0) {
            return false;
        }

        mmio_write(*device_, iir_reg, pending);
        mmio_post(*device_, iir_reg);

        // A flip-done bit completes its own plane. A vblank while armed
        // means every flip written before it has latched.
        const bool vblank = (pending & DE_PIPE_VBLANK_MASK) != 0;
        for (u32 i = 0; i < PLANES_PER_PIPE; i++) {
            const u32 done_mask = de_pipe_flip_done_mask(static_cast<PlaneId>(i));
            if (vblank || (pending & done_mask)) {
                planes_[i].on_flip_complete();
            }
        }

        if (!any_flip_pending()) {
            vblank_disarm_locked();
        }

        return true;
    }

}  // namespace gpu::intel::display
