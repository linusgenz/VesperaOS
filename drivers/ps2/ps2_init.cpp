/**
 * @file ps2_init.cpp
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 10.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
 */

#include <drivers/ps2/mouse.h>

#include "../../arch/x86_64/interrupts/ioapic.h"
#include "keyboard/keyboard_device.h"
#include "keyboard/ps2_keyboard.h"
#include "mouse/mouse_device.h"
#include "ps2_controller.h"

static Ps2Controller* g_ps2 = nullptr;
static Ps2KeyboardDevice* g_kbd = nullptr;
static Ps2MouseDevice* g_mouse = nullptr;

void ps2_init() {
    g_ps2 = new Ps2Controller();

    g_kbd = new Ps2KeyboardDevice(g_ps2);
    g_mouse = new Ps2MouseDevice(g_ps2);

    ps2::keyboard::init();
    ps2::mouse::Ps2Mouse::init();

    const u8 bsp = static_cast<u8>(kernel::acpi::madt::bsp_apic_id());

    // PS/2 Keyboard: IRQ1 → vector 0x21
    arch::x86_64::interrupts::ioapic::configure_irq(1, 0x21, bsp);

    // PS/2 Mouse: IRQ12 → vector 0x2C
    arch::x86_64::interrupts::ioapic::configure_irq(12, 0x22, bsp);
}
