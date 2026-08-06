// intel_rcs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 05.08.26.
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
#ifndef VESPERAOS_INTEL_RCS_H
#define VESPERAOS_INTEL_RCS_H

#include "gfx_pipeline_regs.h"
#include "../intel_engine.h"
#include "../intel_forcewake.h"

namespace blt {

    // RCS MMIO range: 0x2000-0x27FF (PRM Vol 6, "Render Engine Command
    // Streamer (RCS)"). Passed as this engine's MMIO offset to IntelEngine.
    constexpr u32 RCS_ENGINE_OFFSET = 0x2000;

    // ForceWake — Render domain (Gen9 splits ForceWake per engine; this is a
    // *different* request/ack register pair than BCS's FORCEWAKE_BLITTER in
    // intel_bcs.h — confirmed against the public Gen9 register layout:
    // FORCEWAKE_RENDER_GEN9 / FORCEWAKE_ACK_RENDER_GEN9).
    constexpr u32 FORCEWAKE_RENDER = 0xA278;
    constexpr u32 FORCEWAKE_ACK_RENDER = 0x0D84;
    constexpr u32 FORCEWAKE_RENDER_ENABLE = 0x00010001;
    constexpr u32 FORCEWAKE_RENDER_TIMEOUT = 1000;

    constexpr u32 RCS_RING_BUFFER_SIZE = 64u * 1024u;

    /**
     * @brief Render Command Streamer engine.
     *
     * Peer of IntelBcs — both inherit IntelEngine and borrow the same
     * IntelGpuDevice for MMIO base and GGTT, so RCS gets real GPU-memory
     * allocation (via ggtt()/hwsp_alloc()/ring_alloc_and_init()) from the
     * same call it always should have.
     *
     * Current state — Milestone 1 only: ForceWake + a legacy ring buffer +
     * HWSP, enough to verify the Render engine responds to MMIO and can hold
     * a ring buffer, before Execlists (GFX_MODE, LRCA, Context Descriptor,
     * ELSP) are wired up. No PPGTT, no 3DSTATE, no submission helper yet —
     * those are the next steps once this is confirmed working on real
     * Kaby Lake hardware.
     */
    class IntelRcs final : public IntelEngine {
       public:
        explicit IntelRcs(IntelGpuDevice& device);

        IntelRcs(const IntelRcs&) = delete;
        IntelRcs& operator=(const IntelRcs&) = delete;

        bool init_device();

    private:
        bool select_pipeline(PIPELINE_SELECT::PipelineSelection mode);
        bool state_base_address_setup();
        void emit_flush(u32 seqno);
        void debug_dump_hwsp_changes(u32 target_seqno, u32 duration_ms);

        virt_addr_t state_base_cpu_addr_{};
        gfx_addr_t state_base_gfx_addr_{};

        static constexpr u32 STATE_BASE_PAGES = 4;
    };

}  // namespace blt

#endif  // VESPERAOS_INTEL_RCS_H
