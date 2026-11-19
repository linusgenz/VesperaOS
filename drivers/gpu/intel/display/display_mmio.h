// display_mmio.h
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

#ifndef VESPERAOS_DISPLAY_MMIO_H
#define VESPERAOS_DISPLAY_MMIO_H

#include <vespera/types.h>

#include <gpu/intel/core/intel_gpu_device.h>

namespace gpu::intel::display {

    // Thin MMIO helpers for the display block. Display registers are not
    // engine-relative (unlike IntelEngine::engine_reg_*), so they go straight
    // through the device's BAR0 mapping.

    [[nodiscard]] inline u32 mmio_read(const core::IntelGpuDevice& device, const u32 reg) {
        return *reinterpret_cast<volatile u32*>(device.mmio_base() + reg);
    }

    inline void mmio_write(const core::IntelGpuDevice& device, const u32 reg, const u32 value) {
        *reinterpret_cast<volatile u32*>(device.mmio_base() + reg) = value;
    }

    template <typename T>
    [[nodiscard]] T mmio_read(const core::IntelGpuDevice& device, const u32 reg) {
        T val;
        val.raw = mmio_read(device, reg);
        return val;
    }

    template <typename T>
    void mmio_write(const core::IntelGpuDevice& device, const u32 reg, const T val) {
        mmio_write(device, reg, val.raw);
    }

    /// Read-back to flush posted writes.
    inline void mmio_post(const core::IntelGpuDevice& device, const u32 reg) {
        volatile u32 post = mmio_read(device, reg);
        (void)post;
    }

}  // namespace gpu::intel::display

#endif  // VESPERAOS_DISPLAY_MMIO_H
