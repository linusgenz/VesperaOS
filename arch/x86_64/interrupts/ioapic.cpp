// ioapic.cpp
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

#include "ioapic.h"

#include <vespera/log.h>
#include <vespera/mm/memory.h>

namespace arch::x86_64::interrupts::ioapic {

    static volatile u32* map_ioapic(const uptr phys_address) {
        const phys_addr_t phys = make_phys(phys_address);
        const virt_addr_t virt = phys_to_virt(phys);
        kernel::memory::map_memory(virt, phys, (1ULL << PtFlag::CacheDisabled) | (1ULL << PtFlag::ReadWrite));
        return reinterpret_cast<volatile u32*>(virt_raw(virt));
    }

    static u32 ioapic_read(volatile u32* base, const u8 reg) {
        base[IOAPIC_REGSEL] = reg;
        return base[IOAPIC_WINDOW];
    }

    static void ioapic_write(volatile u32* base, const u8 reg, const u32 val) {
        base[IOAPIC_REGSEL] = reg;
        base[IOAPIC_WINDOW] = val;
    }

    u32 get_max_redirects(const kernel::acpi::madt::io_apic* ioapic) {
        volatile u32* mmio = map_ioapic(ioapic->address);
        // Bits [23:16] of the VER register hold (max_redir_entry), which is the number of entries minus one.
        const u32 ver = ioapic_read(mmio, IOAPIC_REG_VER);
        return ((ver >> 16) & 0xFF) + 1;
    }

    static kernel::acpi::madt::io_apic* find_ioapic_for_gsi(const u32 gsi) {
        kernel::acpi::madt::io_apic* apics = kernel::acpi::madt::ioapics();
        for (u32 i = 0; i < kernel::acpi::madt::ioapic_count(); ++i) {
            kernel::acpi::madt::io_apic& apic = apics[i];
            const u32 max_entries = get_max_redirects(&apic);
            if (gsi >= apic.gsi_base && gsi < apic.gsi_base + max_entries) {
                return &apic;
            }
        }
        return nullptr;
    }

    static u32 resolve_irq_to_gsi(const u8 irq) {
        const kernel::acpi::madt::interrupt_override* overrides = kernel::acpi::madt::overrides();
        for (u32 i = 0; i < kernel::acpi::madt::override_count(); ++i) {
            if (overrides[i].source_irq == irq) {
                return overrides[i].gsi;
            }
        }
        return irq;  // identity mapping if no override exists
    }

    static u16 get_flags_for_irq(const u8 irq) {
        const kernel::acpi::madt::interrupt_override* overrides = kernel::acpi::madt::overrides();
        for (u32 i = 0; i < kernel::acpi::madt::override_count(); ++i) {
            if (overrides[i].source_irq == irq) {
                return overrides[i].flags;
            }
        }
        return 0;
    }

    /**
     * Build the low 32 bits of a redirection entry from a vector, MADT flags,
     * and masked state.
     *
     * MADT override flags layout (ACPI spec 6.4, §5.2.12.5):
     *   bits [1:0]  polarity  — 00/01 = bus default/active-high, 11 = active-low
     *   bits [3:2]  trigger   — 00/01 = bus default/edge,        11 = level
     */
    static u32 build_redir_low(const u8 vector, const u16 madt_flags, const bool masked) {
        u32 low = vector;
        low |= 0u << 8;   // delivery mode = Fixed (000)
        low |= 0u << 11;  // destination mode = Physical

        // Polarity: active-low if bits[1:0] == 11
        if ((madt_flags & MADT_FLAG_POLARITY_MASK) == MADT_FLAG_POLARITY_LOW) {
            low |= IOAPIC_REDIR_POLARITY_LOW;
        }

        // Trigger: level-triggered if bits[3:2] == 11
        if ((madt_flags & MADT_FLAG_TRIGGER_MASK) == MADT_FLAG_TRIGGER_LEVEL) {
            low |= IOAPIC_REDIR_TRIGGER_LEVEL;
        }

        if (masked) {
            low |= IOAPIC_REDIR_MASKED;
        }

        return low;
    }

    static void write_redir_entry(volatile u32* mmio, const u32 index, const u32 low, const u32 high) {
        const u8 reg = static_cast<u8>(IOAPIC_REDTBL_BASE + index * 2);
        ioapic_write(mmio, reg + 1, high);
        ioapic_write(mmio, reg, low);
    }

    void configure_irq(const u8 irq, const u8 vector, const u8 dest_apic_id) {
        const u32 gsi = resolve_irq_to_gsi(irq);
        const u16 flags = get_flags_for_irq(irq);

        const kernel::acpi::madt::io_apic* ioapic = find_ioapic_for_gsi(gsi);
        if (!ioapic) {
            Log::error("IOAPIC: No IOAPIC found for GSI %u (IRQ %u)", gsi, irq);
            return;
        }

        volatile u32* mmio = map_ioapic(ioapic->address);
        const u32 index = gsi - ioapic->gsi_base;
        const u32 low = build_redir_low(vector, flags, false /*unmasked*/);
        const u32 high = static_cast<u32>(dest_apic_id) << 24;

        write_redir_entry(mmio, index, low, high);

        Log::info(
            "IOAPIC: IRQ %u -> GSI %u -> vec 0x%x on APIC %u (flags 0x%x)", irq, gsi, vector, dest_apic_id, flags
        );
    }

    void mask_gsi(const u32 gsi) {
        const kernel::acpi::madt::io_apic* ioapic = find_ioapic_for_gsi(gsi);
        if (!ioapic) return;

        volatile u32* mmio = map_ioapic(ioapic->address);
        const u8 reg = static_cast<u8>(IOAPIC_REDTBL_BASE + (gsi - ioapic->gsi_base) * 2);
        const u32 low = ioapic_read(mmio, reg);
        ioapic_write(mmio, reg, low | IOAPIC_REDIR_MASKED);
    }

    void unmask_gsi(const u32 gsi) {
        const kernel::acpi::madt::io_apic* ioapic = find_ioapic_for_gsi(gsi);
        if (!ioapic) return;

        volatile u32* mmio = map_ioapic(ioapic->address);
        const u8 reg = static_cast<u8>(IOAPIC_REDTBL_BASE + (gsi - ioapic->gsi_base) * 2);
        const u32 low = ioapic_read(mmio, reg);
        ioapic_write(mmio, reg, low & ~IOAPIC_REDIR_MASKED);
    }

    void init() {
        Log::info("IOAPIC: Initializing %u IOAPIC(s)", kernel::acpi::madt::ioapic_count());

        // All legacy irqs are masked by default
        // Subsystems call configure_irq() to activate what they need.
        const u8 bsp = static_cast<u8>(kernel::acpi::madt::bsp_apic_id());
        for (u8 irq = 0; irq < 16; ++irq) {
            const u32 gsi = resolve_irq_to_gsi(irq);
            const kernel::acpi::madt::io_apic* ioapic = find_ioapic_for_gsi(gsi);
            if (!ioapic) continue;

            const u16 flags = get_flags_for_irq(irq);
            volatile u32* mmio = map_ioapic(ioapic->address);
            const u32 index = gsi - ioapic->gsi_base;
            // Vector 0x20 + irq is the standard PIC-compatible mapping.
            const u32 low = build_redir_low(0x20 + irq, flags, true);
            const u32 high = static_cast<u32>(bsp) << 24;
            write_redir_entry(mmio, index, low, high);
        }
    }

}  // namespace arch::x86_64::interrupts::ioapic