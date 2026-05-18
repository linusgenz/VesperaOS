// interrupt_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 18.05.26.
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
#ifndef VESPERAOS_INTERRUPT_REGS_H
#define VESPERAOS_INTERRUPT_REGS_H

// https://kiwitree.net/~lina/intel-gfx-docs/prm/kbl/intel-gfx-prm-osrc-kbl-vol02c-commandreference-registers-part1.pdf (Page 1080)
// https://kiwitree.net/~lina/intel-gfx-docs/prm/kbl/intel-gfx-prm-osrc-kbl-vol02d-commandreference-structures.pdf (Page 38)

struct BCS_IMR_BITS {
    u32 reserved0_15 : 16;

    /**
     * @brief Blitter Command Parser User Interrupt [16]
     */
    u32 user_interrupt : 1;

    u32 reserved17_18 : 2;

    /**
     * @brief Blitter Command Parser Master Error [19]
     */
    u32 master_error : 1;

    /**
     * @brief MI_FLUSH_DW Notify Interrupt [20]
     */
    u32 mi_flush_dw : 1;

    u32 reserved21 : 1;

    /**
     * @brief Timeout Counter Expired [22]
     *
     * WARNING: MUST NOT be unmasked in production.
     */
    u32 timeout_expired : 1;

    u32 reserved23 : 1;

    /**
     * @brief Context Switch Interrupt [24]
     */
    u32 context_switch : 1;

    u32 reserved25_26 : 2;

    /**
     * @brief Wait on Semaphore Interrupt [27]
     */
    u32 wait_on_semaphore : 1;

    u32 reserved28_31 : 4;
} __attribute__((packed));

static_assert(sizeof(BCS_IMR_BITS) == 4);

union IMR_REG {
    struct {
        BCS_IMR_BITS bcs;
    } __attribute__((packed));

    u32 raw;
};

static_assert(sizeof(IMR_REG) == 4);

#endif  // VESPERAOS_INTERRUPT_REGS_H
