// lrc_layout.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 27.08.26.
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

#ifndef VESPERAOS_LRC_LAYOUT_H
#define VESPERAOS_LRC_LAYOUT_H

#include <vespera/types.h>

namespace gpu::intel::core {

    // -------------------------------------------------------------------------------------------
    // LRC (Logical Ring Context) layout, per KBL PRM Vol 6 ("Command Stream Programming") and
    // Vol 7 ("3D-Media_GPGPU", Context Image / Register Information). Identical DWord-offset
    // shape across RCS/BCS/VCS/VECS - only the per-DWord MMIO value differs per engine, and that
    // value is whatever the engine already writes via engine_reg_write() at those same relative
    // offsets (ENGINE_RING_TAIL_OFF etc. in intel_engine.h).
    //
    // Only the Ring Context portion is modeled here. The much larger, engine-specific Engine
    // Context that follows it (pipeline/non-pipeline state, PRM Vol 7 pg. 21 onward, ~20K DWords
    // for RCS alone) is intentionally NOT modeled: the PRM states its format is device-dependent
    // and "software must not access the memory contents directly" - the engine populates it
    // itself on first context save when Engine Context Restore Inhibit is set (see
    // LRC_CTX_SR_CTL_FIRST_SUBMIT_BITS below).
    // -------------------------------------------------------------------------------------------

    constexpr usize LRC_PAGE_SIZE = 0x1000;

    /// Per-Process HW Status Page occupies LRC page 0. Ring Context starts at page 1 (LRCA+0x1000).
    constexpr usize LRC_PPHWSP_SIZE = LRC_PAGE_SIZE;
    constexpr usize LRC_RING_CONTEXT_START = LRC_PAGE_SIZE;

    /// Ring Context is five cachelines (PRM: "Ring context is five cachelines in size").
    constexpr usize LRC_CACHELINE_SIZE = 64;
    constexpr usize LRC_RING_CONTEXT_SIZE = 5 * LRC_CACHELINE_SIZE;  ///< 320 bytes = 80 DWords

    /// LRC total size for engines with Execlists enabled and no large engine context (BCS/VCS/VECS):
    /// PPHWSP (1 page) + Ring Context, rounded up to whole pages -> 2 pages total per PRM Vol 6/3.
    constexpr usize LRC_SIZE_SMALL_ENGINE = 2 * LRC_PAGE_SIZE;

    /// RCS LRC total size: PPHWSP + Ring Context + Engine Context = 20 pages per PRM Vol 7.
    constexpr usize LRC_SIZE_RCS = 22 * LRC_PAGE_SIZE;

    // Ring Context DWord offsets, relative to LRC_RING_CONTEXT_START (i.e. relative to LRCA+0x1000).
    // These match the "Address Offset (Dword)" column in the PRM tables and are identical across
    // RCS/BCS/VCS/VECS.
    constexpr usize LRC_DW_CONTEXT_CONTROL = 0x0002;
    constexpr usize LRC_DW_RING_HEAD = 0x0004;
    constexpr usize LRC_DW_RING_TAIL = 0x0006;
    constexpr usize LRC_DW_RING_BUFFER_START = 0x0008;
    constexpr usize LRC_DW_RING_BUFFER_CONTROL = 0x000A;
    constexpr usize LRC_DW_BB_CURRENT_HEAD_UDW = 0x000C;
    constexpr usize LRC_DW_BB_CURRENT_HEAD = 0x000E;
    constexpr usize LRC_DW_BB_STATE = 0x0010;

    /// BB_STATE Address Space Indicator bit (engine-relative MMIO 0x110). Confirmed against
    /// Fuchsia's msd-intel-gen driver (register_state_helper.h): set unconditionally for every
    /// Execlist context regardless of whether PPGTT is actually programmed - this is what tells
    /// the engine which addressing mode to interpret batch-buffer addresses in, and per Fuchsia's
    /// reference, an Execlist context load/activation can fail outright if it's left at 0.
    constexpr u32 LRC_BB_STATE_ADDRESS_SPACE_PPGTT_BIT = 1u << 5;


    constexpr usize LRC_DW_SECOND_BB_ADDR_UDW = 0x0012;
    constexpr usize LRC_DW_SECOND_BB_ADDR = 0x0014;
    constexpr usize LRC_DW_SECOND_BB_STATE = 0x0016;
    constexpr usize LRC_DW_BB_PER_CTX_PTR = 0x0018;
    constexpr usize LRC_DW_INDIRECT_CTX = 0x001A;         ///< RCS_INDIRECT_CTX / BCS_INDIRECT_CTX / ...
    constexpr usize LRC_DW_INDIRECT_CTX_OFFSET = 0x001C;  ///< ..._INDIRECT_CTX_OFFSET, same shape per engine

    constexpr usize LRC_DW_CTX_TIMESTAMP = 0x0022;

    // -------------------------------------------------------------------------------------------
    // CTXT_SR_CTL - Context Save/Restore Control Register (LRC_DW_CONTEXT_CONTROL / MMIO 0x244
    // engine-relative, e.g. 0x2244 absolute for RCS). Masked-write register: upper word is the
    // write mask, lower word the value - same shape as GFX_MODE_EXECLIST_ENABLE_MASK_AND_VALUE
    // in intel_engine.h. Bit positions confirmed against both Fuchsia's and Xe's drivers.
    //
    // Only RENDER_CONTEXT_RESTORE_INHIBIT and INHIBIT_SYNC_CONTEXT_SWITCH are set, and only for
    // RCS, and only on the context's first-ever submit: this tells HW to skip restoring the
    // (still-unpopulated) Engine Context on that first load, instead of restoring garbage/zeros
    // into pipeline state. HW populates the Engine Context itself on the first context *save*
    // that follows. Submits after the first must NOT set this again, or every restore for this
    // context would be skipped forever.
    // -------------------------------------------------------------------------------------------
    constexpr u32 LRC_CTX_SR_CTL_RESTORE_INHIBIT_BIT = 1u << 0;
    constexpr u32 LRC_CTX_SR_CTL_INHIBIT_SYNC_CTX_SWITCH_BIT = 1u << 3;
    constexpr u32 LRC_CTX_SR_CTL_FIRST_SUBMIT_BITS =
        LRC_CTX_SR_CTL_RESTORE_INHIBIT_BIT | LRC_CTX_SR_CTL_INHIBIT_SYNC_CTX_SWITCH_BIT;
    constexpr u32 LRC_CTX_SR_CTL_FIRST_SUBMIT_MASK_AND_VALUE =
        (LRC_CTX_SR_CTL_FIRST_SUBMIT_BITS << 16) | LRC_CTX_SR_CTL_FIRST_SUBMIT_BITS;

    /// PDP (Page Directory Pointer) descriptors for PPGTT. Written high-DWord-then-low-DWord per
    /// entry, PDP3 first, PDP0 last - matches the PRM table row order exactly. In 64-bit (48-bit
    /// canonical) addressing mode only PDP0 is used (holds the PML4 base); PDP1-3 are ignored by
    /// HW but the PRM still shows them zeroed via the same LOAD_REGISTER_IMM block, so we write
    /// all four for a well-defined context image.
    constexpr usize LRC_DW_PDP3_UDW = 0x0024;
    constexpr usize LRC_DW_PDP3_LDW = 0x0026;
    constexpr usize LRC_DW_PDP2_UDW = 0x0028;
    constexpr usize LRC_DW_PDP2_LDW = 0x002A;
    constexpr usize LRC_DW_PDP1_UDW = 0x002C;
    constexpr usize LRC_DW_PDP1_LDW = 0x002E;
    constexpr usize LRC_DW_PDP0_UDW = 0x0030;
    constexpr usize LRC_DW_PDP0_LDW = 0x0032;

    /// RCS-only third LRI block: R_PWR_CLK_STATE (Render Power Clock State), one register.
    /// Confirmed against Fuchsia's msd-intel-gen driver, which gates this on
    /// RENDER_COMMAND_STREAMER specifically - not part of the shared Ring Context shape that
    /// BCS/VCS/VECS use.
    constexpr usize LRC_DW_RENDER_PWR_CLK_STATE = 0x0042;
    constexpr u32 ENGINE_RENDER_PWR_CLK_STATE_OFF = 0xC8;
    constexpr u32 LRC_LRI_HEADER_RENDER_PWR_CLK_BLOCK = 0x1100'0001;  ///< precedes R_PWR_CLK_STATE (1 reg, force_posted=false)


    // Corresponding engine-relative MMIO offsets (add to engine_mmio_offset_ / read via
    // engine_regs()), taken directly from the PRM Vol 7 RCS "Register Information" table (e.g.
    // PDP0_LDW = 0x2270 absolute, i.e. 0x270 relative to the RCS base of 0x2000) - matching
    // engine_reg_write()'s existing relative-offset style. The same relative offsets apply
    // unchanged to BCS/VCS/VECS per PRM Vol 3 (each engine's absolute MMIO base differs, but the
    // Ring Context field layout within that base does not).
    constexpr u32 ENGINE_CONTEXT_CONTROL_OFF = 0x244;
    constexpr u32 ENGINE_BB_CURRENT_HEAD_UDW_OFF = 0x168;
    constexpr u32 ENGINE_BB_CURRENT_HEAD_OFF = 0x140;
    constexpr u32 ENGINE_BB_STATE_OFF = 0x110;
    constexpr u32 ENGINE_SECOND_BB_ADDR_UDW_OFF = 0x11C;
    constexpr u32 ENGINE_SECOND_BB_ADDR_OFF = 0x114;
    constexpr u32 ENGINE_SECOND_BB_STATE_OFF = 0x118;
    constexpr u32 ENGINE_BB_PER_CTX_PTR_OFF = 0x1C0;
    constexpr u32 ENGINE_INDIRECT_CTX_OFF = 0x1C4;
    constexpr u32 ENGINE_INDIRECT_CTX_OFFSET_OFF = 0x1C8;
    constexpr u32 ENGINE_CTX_TIMESTAMP_OFF = 0x3A8;
    constexpr u32 ENGINE_PDP0_LDW_OFF = 0x270;
    constexpr u32 ENGINE_PDP0_UDW_OFF = 0x274;
    constexpr u32 ENGINE_PDP1_LDW_OFF = 0x278;
    constexpr u32 ENGINE_PDP1_UDW_OFF = 0x27C;
    constexpr u32 ENGINE_PDP2_LDW_OFF = 0x280;
    constexpr u32 ENGINE_PDP2_UDW_OFF = 0x284;
    constexpr u32 ENGINE_PDP3_LDW_OFF = 0x288;
    constexpr u32 ENGINE_PDP3_UDW_OFF = 0x28C;

    /// MI_LOAD_REGISTER_IMM header format embedded in the LRC. The PRM shows each Ring Context
    /// block prefixed with one of these (e.g. 0x1100_101B before the ring/BB block, 0x1100_1011
    /// before the PDP block) - the low 9 bits encode how many (register, value) DWord pairs
    /// follow. We only ever need the two blocks actually covered above.
    constexpr u32 LRC_LRI_HEADER_RING_BLOCK = 0x1100'101B;  ///< precedes Context Control .. Indirect Ctx Offset (14 regs)
    constexpr u32 LRC_LRI_HEADER_PDP_BLOCK = 0x1100'1011;   ///< precedes CTX_TIMESTAMP .. PDP0_LDW (9 regs)

}  // namespace gpu::intel::core

#endif  // VESPERAOS_LRC_LAYOUT_H
