// gt_reset_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 24.05.26.
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
#ifndef VESPERAOS_GT_RESET_REGS_H
#define VESPERAOS_GT_RESET_REGS_H

constexpr u32 GDRST_MMIO = 0x941C;

/**
 * @brief Graphics Device Reset Control register (GDRST).
 *
 * @note This is a GT-level reset register (not engine-local).
 *       It can reset multiple GPU domains: render, blitter, media, vebox, SFC.
 *
 * @note Bits are R/W1S (write-1-to-set). Writing '1' initiates a reset request.
 *       Hardware clears the bit automatically when reset is complete.
 *
 * @note There is NO software-clear semantic:
 *       writing '0' has no effect, and read-modify-write is only safe if
 *       restricted to setting bits.
 *
 * @note This is a non-posted register. Completion must be polled.
 */
union GDRST {
    struct {
        u32 full_reset : 1;     ///< [0]  Full GT reset (all domains)
        u32 render     : 1;     ///< [1]  Render (RCS) reset
        u32 media      : 1;     ///< [2]  Media0 reset
        u32 blitter    : 1;     ///< [3]  Blitter (BCS) reset
        u32 vebox      : 1;     ///< [4]  VEBox reset
        u32 reserved5  : 1;     ///< [5]  MBZ
        u32 reserved6  : 1;     ///< [6]  MBZ
        u32 media1     : 1;     ///< [7]  Media1 / extended media domain
        u32 sfc0       : 1;     ///< [8]  SFC0 reset
        u32 sfc1       : 1;     ///< [9]  SFC1 reset
        u32 reserved10_31 : 22; ///< [31:10] MBZ / RO
    } __attribute__((packed));

    u32 raw;
};

#endif  // VESPERAOS_GT_RESET_REGS_H
