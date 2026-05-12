// sleep_timer.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 17.04.26.
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

#include <acpi/madt.h>
#include <vespera/time.h>

#include <arch/x86_64/interrupts/apic.h>

namespace kernel::time::sleep_timer {

    namespace {
        struct CpuTimerState {
            // Absolute uptime (ns) when the current scheduler quantum ends.
            u64 quantum_deadline_ns = 0;

            // Absolute uptime (ns) of the earliest sleeping unit in the
            // blocked queue.  0 means "no sleeping units pending".
            u64 next_sleep_wakeup_ns = 0;
        };

        CpuTimerState g_state[kernel::acpi::madt::MAX_CPU_CORES];
    }  // namespace

    void start(const u8 cpu_id) {
        const u64 now = get_uptime_ns();
        g_state[cpu_id].quantum_deadline_ns = now + arch::x86_64::interrupts::apic::APIC_QUANTUM_NS;
        g_state[cpu_id].next_sleep_wakeup_ns = 0;
        arch::x86_64::interrupts::apic::arm_oneshot_ns(arch::x86_64::interrupts::apic::APIC_QUANTUM_NS);
    }

    void notify_sleep(const u8 cpu_id, const u64 wakeup_ns) {
        auto& st = g_state[cpu_id];
        if (st.next_sleep_wakeup_ns == 0 || wakeup_ns < st.next_sleep_wakeup_ns) {
            st.next_sleep_wakeup_ns = wakeup_ns;
        }
    }

    void update_min_wakeup(const u8 cpu_id, const u64 new_min_ns) {
        g_state[cpu_id].next_sleep_wakeup_ns = new_min_ns;
    }

    void set_quantum_deadline(const u8 cpu_id, const u64 deadline_ns) {
        g_state[cpu_id].quantum_deadline_ns = deadline_ns;
    }

    void arm_next_event(const u8 cpu_id) {
        const u64 now = kernel::time::get_uptime_ns();
        const auto& st = g_state[cpu_id];

        u64 next_ns = (st.quantum_deadline_ns > now) ? st.quantum_deadline_ns
                                                     : now + arch::x86_64::interrupts::apic::APIC_QUANTUM_NS;

        if (st.next_sleep_wakeup_ns != 0 && st.next_sleep_wakeup_ns < next_ns) {
            next_ns = st.next_sleep_wakeup_ns;
        }

        const u64 delay_ns = (next_ns > now) ? (next_ns - now) : arch::x86_64::interrupts::apic::APIC_MIN_DELAY_NS;

        arch::x86_64::interrupts::apic::arm_oneshot_ns(delay_ns);
    }

}  // namespace kernel::time::sleep_timer