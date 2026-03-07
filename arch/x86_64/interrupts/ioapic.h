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
#include "../../../kernel/acpi/madt.h"

namespace arch::x86_64::interrupts::ioapic {
#define IOAPIC_REGSEL 0x00
#define IOAPIC_WINDOW 0x10

    void init();

    void configure_irq(u8 irq, u8 vector, u8 dest_apic_id);

}  // namespace arch::x86_64::interrupts::ioapic

#endif  // IOAPIC_H
