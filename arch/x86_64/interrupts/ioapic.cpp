// ioapic.cpp
//
// LuminOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 23.07.25.
//
// This file is part of LuminOS.
// 
// LuminOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// LuminOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with LuminOS. If not, see <https://www.gnu.org/licenses/>.

#include "ioapic.h"
#include "../../../include/log.h"
#include "../../../kernel/include/page_table_manager.h"

static MADT::IoApic* find_ioapic_for_gsi(uint32_t gsi) {
    MADT::IoApic *apics = MADT::get_ioapics();
    for (uint32_t i = 0; i < MADT::get_ioapic_count(); ++i) {
        auto& apic = apics[i];
        if (gsi >= apic.gsi_base && gsi < apic.gsi_base + 24) {
            return &apic;
        }
    }
    return nullptr;
}

static uint32_t resolve_irq_to_gsi(uint8_t irq) {
    MADT::InterruptOverride *overrides = MADT::get_overrides();
    for (uint32_t i = 0; i < MADT::get_override_count(); ++i) {
        if (overrides[i].source_irq == irq) {
            return overrides[i].gsi;
        }
    }
    return irq;
}

static uint16_t get_flags_for_irq(uint8_t irq) {
    MADT::InterruptOverride *overrides = MADT::get_overrides();
    for (uint32_t i = 0; i < MADT::get_override_count(); ++i) {
        if (overrides[i].source_irq == irq) {
            return overrides[i].flags;
        }
    }
    return 0; // default flags: polarity = high, trigger = edge
}

static volatile uint32_t *map_ioapic(uintptr_t address) {
    global_page_table_manager.map_memory ((void*)address, (void*)address);
    return reinterpret_cast<volatile uint32_t*>(address);
}

static void write_ioapic_reg(volatile uint32_t* base, uint8_t reg, uint32_t val) {
    base[IOAPIC_REGSEL] = reg;
    base[IOAPIC_WINDOW] = val;
}

static void ioapic_set_redirect(MADT::IoApic* ioapic, uint32_t gsi, uint8_t vector, uint8_t dest_apic_id, uint16_t flags) {
    volatile uint32_t* mmio = map_ioapic(ioapic->address);
    uint32_t index = gsi - ioapic->gsi_base;
    uint8_t reg = 0x10 + (index * 2);

    uint32_t low = vector;
    low |= 0 << 8;  // delivery mode fixed
    low |= 0 << 11; // physical
    low |= ((flags >> 1) & 1) << 13; // polarity
    low |= ((flags >> 3) & 1) << 15; // trigger mode
    low |= 0 << 16; // mask = 0 (enabled)

    uint32_t high = dest_apic_id << 24;

    low |= 1 << 16; // masked
    write_ioapic_reg(mmio, reg, low);
    write_ioapic_reg(mmio, reg + 1, high);

    low &= ~(1 << 16);
    write_ioapic_reg(mmio, reg, low);

    Log::Info("IOAPIC: Redirect GSI %u (IRQ 0x%x) -> vec 0x%x on CPU %u (flags: 0x%x)",
              gsi, gsi, vector, dest_apic_id, flags);
}

void IOAPIC::configure_irq(uint8_t irq, uint8_t vector, uint8_t dest_apic_id) {
    uint32_t gsi = resolve_irq_to_gsi(irq);
    uint16_t flags = get_flags_for_irq(irq);

    MADT::IoApic* ioapic = find_ioapic_for_gsi(gsi);
    if (!ioapic) {
        Log::Error("IOAPIC: No APIC found for GSI %u (IRQ 0x%x)", gsi, irq);
        return;
    }

    ioapic_set_redirect(ioapic, gsi, vector, dest_apic_id, flags);
}

void IOAPIC::init() {
    const uint8_t default_irqs[] = {0, 5, 9, 10, 11}; // default irq's

    for (uint8_t irq : default_irqs) {
        configure_irq(irq, 0x20 + irq, MADT::get_bsp_apic_id());
    }
}
