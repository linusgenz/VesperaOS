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
    static madt::IoApic *find_ioapic_for_gsi(const u32 gsi) {
        madt::IoApic *apics = madt::get_ioapics();
        for (u32 i = 0; i < madt::get_ioapic_count(); ++i) {
            auto &apic = apics[i];
            if (gsi >= apic.gsi_base && gsi < apic.gsi_base + 24) {
                return &apic;
            }
        }
        return nullptr;
    }

    static u32 resolve_irq_to_gsi(const u8 irq) {
        const madt::InterruptOverride *overrides = madt::get_overrides();
        for (u32 i = 0; i < madt::get_override_count(); ++i) {
            if (overrides[i].source_irq == irq) {
                return overrides[i].gsi;
            }
        }
        return irq;
    }

    static u16 get_flags_for_irq(const u8 irq) {
        const madt::InterruptOverride *overrides = madt::get_overrides();
        for (u32 i = 0; i < madt::get_override_count(); ++i) {
            if (overrides[i].source_irq == irq) {
                return overrides[i].flags;
            }
        }
        return 0;  // default flags: polarity = high, trigger = edge
    }

    static volatile u32 *map_ioapic(const uptr address) {
        kernel::memory::map_memory(phys_to_virt(make_phys(address)),make_phys(address));
        return reinterpret_cast<volatile u32 *>(address);
    }

    static void write_ioapic_reg(volatile u32 *base, const u8 reg, const u32 val) {
        base[IOAPIC_REGSEL] = reg;
        base[IOAPIC_WINDOW] = val;
    }

    static void ioapic_set_redirect(
        const madt::IoApic *ioapic, const u32 gsi, const u8 vector, const u8 dest_apic_id, const u16 flags
    ) {
        volatile u32 *mmio = map_ioapic(ioapic->address);
        const u32 index = gsi - ioapic->gsi_base;
        const u8 reg = 0x10 + (index * 2);

        u32 low = vector;
        low |= 0 << 8;                    // delivery mode fixed
        low |= 0 << 11;                   // physical
        low |= ((flags >> 1) & 1) << 13;  // polarity
        low |= ((flags >> 3) & 1) << 15;  // trigger mode
        low |= 0 << 16;                   // mask = 0 (enabled)

        const u32 high = dest_apic_id << 24;

        low |= 1 << 16;  // masked
        write_ioapic_reg(mmio, reg, low);
        write_ioapic_reg(mmio, reg + 1, high);

        low &= ~(1 << 16);
        write_ioapic_reg(mmio, reg, low);

        Log::info(
            "IOAPIC: Redirect GSI %u (IRQ 0x%x) -> vec 0x%x on CPU %u (flags: 0x%x)",
            gsi,
            gsi,
            vector,
            dest_apic_id,
            flags
        );
    }

    void configure_irq(const u8 irq, const u8 vector, const u8 dest_apic_id) {
        const u32 gsi = resolve_irq_to_gsi(irq);
        const u16 flags = get_flags_for_irq(irq);

        const madt::IoApic *ioapic = find_ioapic_for_gsi(gsi);
        if (!ioapic) {
            Log::error("IOAPIC: No APIC found for GSI %u (IRQ 0x%x)", gsi, irq);
            return;
        }

        ioapic_set_redirect(ioapic, gsi, vector, dest_apic_id, flags);
    }

    void init() {
        for (const u8 default_irqs[] = {0, 5, 9, 10, 11}; const u8 irq : default_irqs) {
            configure_irq(irq, 0x20 + irq, madt::get_bsp_apic_id());
        }
    }
}  // namespace arch::x86_64::interrupts::ioapic
