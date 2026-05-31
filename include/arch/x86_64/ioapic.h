// ioapic.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 31.05.26.
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
#ifndef VESPERAOS_ARCH_X86_64_IOAPIC_H
#define VESPERAOS_ARCH_X86_64_IOAPIC_H

#include <vespera/types.h>

namespace arch::x86_64::interrupts::ioapic {

    constexpr u8 PIT_ISA_IRQ = 0;

    /**
     * Initialize all IOAPICs found in the MADT.
     * Maps all 16 legacy ISA IRQs to vectors 0x20–0x2F on the BSP,
     * initially masked. Caller should enable specific IRQs via configure_irq().
     */
    void init();

    /**
     * Route a legacy IRQ (0–15) to the given IDT vector on dest_apic_id.
     * Resolves GSI via MADT overrides and unmasks the entry.
     */
    void configure_irq(u8 irq, u8 vector, u8 dest_apic_id);

    /**
     * Mask a GSI in the IOAPIC redirection table (disables the IRQ).
     */
    void mask_gsi(u32 gsi);

    /**
     * Unmask a GSI in the IOAPIC redirection table (enables the IRQ).
     */
    void unmask_gsi(u32 gsi);
}  // namespace arch::x86_64::interrupts::ioapic

#endif  // VESPERAOS_ARCH_X86_64_IOAPIC_H
