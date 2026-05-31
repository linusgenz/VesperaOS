// ioapic.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 23.07.25.
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

#ifndef IOAPIC_H
#define IOAPIC_H
#include <acpi/madt.h>

namespace arch::x86_64::interrupts::ioapic {

    // IOAPIC MMIO register offsets (index into u32 array)
    static constexpr u32 IOAPIC_REGSEL = 0x00 / 4;
    static constexpr u32 IOAPIC_WINDOW = 0x10 / 4;

    // IOAPIC internal register indices
    static constexpr u8 IOAPIC_REG_ID = 0x00;
    static constexpr u8 IOAPIC_REG_VER = 0x01;
    static constexpr u8 IOAPIC_REG_ARB = 0x02;
    static constexpr u8 IOAPIC_REDTBL_BASE = 0x10;

    // Redirection entry flags
    static constexpr u32 IOAPIC_REDIR_MASKED = (1u << 16);
    static constexpr u32 IOAPIC_REDIR_TRIGGER_LEVEL = (1u << 15);
    static constexpr u32 IOAPIC_REDIR_POLARITY_LOW = (1u << 13);
    static constexpr u32 IOAPIC_REDIR_DESTMODE_LOG = (1u << 11);

    // MADT override flag bits
    static constexpr u16 MADT_FLAG_POLARITY_MASK = 0x03;
    static constexpr u16 MADT_FLAG_TRIGGER_MASK = 0x0C;
    static constexpr u16 MADT_FLAG_POLARITY_LOW = 0x03;
    static constexpr u16 MADT_FLAG_TRIGGER_LEVEL = 0x0C;

    /**
     * Read the maximum number of redirection entries from an IOAPIC's VER register.
     * Returns the actual hardware value, not a hardcoded constant.
     */
    u32 get_max_redirects(const kernel::acpi::madt::io_apic* ioapic);

}  // namespace arch::x86_64::interrupts::ioapic

#endif  // IOAPIC_H
