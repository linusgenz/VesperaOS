// ioapic.h
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

#ifndef IOAPIC_H
#define IOAPIC_H
#include "../../../kernel/acpi/madt.h"

#define IOAPIC_REGSEL 0x00
#define IOAPIC_WINDOW 0x10
namespace IOAPIC {

    void init();

    void configure_irq(uint8_t irq, uint8_t vector, uint8_t dest_apic_id);

}

#endif //IOAPIC_H
