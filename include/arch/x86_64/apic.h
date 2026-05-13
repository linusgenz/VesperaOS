// apic.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 13.05.26.
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

#ifndef VESPERAOS_VESPERA_ARCH_APIC_H
#define VESPERAOS_VESPERA_ARCH_APIC_H

#include <vespera/types.h>

namespace arch::x86_64::interrupts::apic {

    /** @brief Default scheduler quantum — 10 ms expressed in nanoseconds. */
    constexpr u64 QUANTUM_NS = 10'000'000ULL;

    // Minimum one-shot delay to avoid re-arming with 0 counts.
    constexpr u64 APIC_MIN_DELAY_NS = 50'000ULL;  // 50 µs

    /**
     * @brief Initializes the Local APIC for the calling CPU.
     *
     * Calibrates the APIC timer against the PMT, programs the spurious
     * vector, and arms the first one-shot tick.
     *
     * @note Must be called once per CPU during SMP bring-up.
     */
    void init(u8 cpu_id);

    /**
     * @brief Same as @ref init() but with additional initialization
     * @see init()
     */
    void init_bsp();

    /**
     * @brief Signals End-Of-Interrupt to the Local APIC.
     *
     * @note Must be called at the end of every APIC-sourced ISR.
     */
    void send_eoi();

    /**
     * @brief Arms the APIC one-shot timer for the given nanosecond delay.
     */
    void arm_oneshot_ns(u64 ns);

    /**
     * @brief Sends an IPI to a specific CPU identified by its APIC ID.
     */
    void send_ipi(u32 apic_id, u8 vector);

    /**
     * @brief Sends an IPI to all online CPUs except the calling CPU.
     */
    void broadcast_ipi(u8 vector);

    /**
     * @brief Stops all CPUs except the calling CPU.
     */
    void halt_cpus();

    /**
     * @brief Returns the APIC ID of the calling CPU.
     */
    [[nodiscard]] u32 get_id();

    /**
     * @brief Sends the INIT IPI sequence to the target AP.
     *
     * Performs INIT assert + deassert with the required 10 ms delays.
     * Must be followed by @ref send_sipi to actually start the AP.
     *
     * @param apic_id  APIC ID of the target Application Processor.
     */
    void send_init_ipi(u32 apic_id);

    /**
     * @brief Sends a Startup IPI (SIPI) to the target AP.
     *
     * @param apic_id  APIC ID of the target AP.
     * @param vector   Startup vector (page-aligned, e.g. 0x08 → 0x8000).
     */
    void send_sipi(u32 apic_id, u8 vector);
}  // namespace arch::x86_64::interrupts::apic

#endif  // VESPERAOS_VESPERA_ARCH_APIC_H