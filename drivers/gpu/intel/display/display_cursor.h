// display_cursor.h
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

#ifndef VESPERAOS_DISPLAY_CURSOR_H
#define VESPERAOS_DISPLAY_CURSOR_H

#include <vespera/mm/addr.h>
#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

#include "display_reg_addr.h"
#include "display_surface.h"

namespace gpu::intel::core {
    class IntelGpuDevice;
}

namespace gpu::intel::display {

    /**
     * @brief Hardware cursor plane of one display pipe (CUR_CTL/BASE/POS/PAL/FBC).
     *
     * Peer of DisplayPlane: plain MMIO programming, two-phase init() because
     * it lives inside DisplayPipe. All cursor registers are double-buffered
     * and armed by writing CUR_BASE last.
     */
    class DisplayCursor {
    public:
        DisplayCursor() = default;

        DisplayCursor(const DisplayCursor&) = delete;
        DisplayCursor& operator=(const DisplayCursor&) = delete;

        void init(core::IntelGpuDevice& device, PipeId pipe_id);

        /// Assigns the reserved DDB block slice for this cursor plane.
        /// Must be set to a non-overlapping slice before commit().
        void set_ddb(DdbAllocation ddb);

        [[nodiscard]] DdbAllocation ddb() const;

        [[nodiscard]] bool has_ddb() const {
            return ddb().valid;
        }

        /// Validates `cfg`, programs CUR_BUF_CFG/CUR_WM/FBC_CTL/POS/CTL and finally CUR_BASE to arm.
        /// Returns false (writing nothing) if any hardware constraint is violated.
        [[nodiscard]] bool commit(const CursorConfig& cfg);

        /// Updates the cursor surface address (keeping current mode/position) and
        /// arms the update via CUR_BASE if the cursor is currently enabled.
        [[nodiscard]] bool set_surface(gfx_addr_t surface);

        /// Updates CUR_POS (sign-magnitude) and re-arms CUR_BASE if enabled.
        [[nodiscard]] bool set_position(i32 x, i32 y) const;

        /// Programs all 4 CUR_PAL entries (for 2 bpp modes) and arms if enabled.
        void set_palette(const CUR_PAL entries[CURSOR_PAL_COUNT]) const;

        /// Disables the cursor (CUR_CTL.mode = MODE_DISABLE) and arms via CUR_BASE.
        void disable();

        /// Reads back the active hardware cursor configuration.
        /// Returns false if the cursor is currently disabled.
        [[nodiscard]] bool read_config(CursorConfig& out) const;

        /// Returns the live GTT address currently being scanned out (CUR_SURFLIVE).
        [[nodiscard]] gfx_addr_t live_surface() const;

        [[nodiscard]] bool enabled() const;

        [[nodiscard]] PipeId pipe_id() const {
            return pipe_id_;
        }

    private:
        [[nodiscard]] static bool validate(const CursorConfig& cfg) ;
        void arm_base_locked(gfx_addr_t surface) const;

        [[nodiscard]] u32 reg(const u32 reg_a) const {
            return cursor_reg(pipe_id_, reg_a);
        }

        core::IntelGpuDevice* device_{nullptr};
        PipeId pipe_id_ = PipeId::A;

        mutable Spinlock lock_;
        DdbAllocation ddb_{};
        gfx_addr_t surface_{};
        CUR_CTL::CursorMode mode_{CUR_CTL::MODE_DISABLE};
        bool enabled_{false};
    };

}  // namespace gpu::intel::display

#endif  // VESPERAOS_DISPLAY_CURSOR_H