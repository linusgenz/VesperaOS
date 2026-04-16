// hpet.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.04.26.
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

#include "hpet.h"

#include <acpi/acpi.h>
#include <vespera/kerrno.h>
#include <vespera/log.h>

#include "vespera/mm/memory.h"

namespace kernel::time {

    u64 HpetClock::reg_read64(const u32 offset) const {
        volatile auto* reg = reinterpret_cast<volatile u64*>(mmio_base_ + offset);
        return *reg;
    }

    void HpetClock::reg_write64(const u32 offset, const u64 value) const {
        volatile auto* reg = reinterpret_cast<volatile u64*>(mmio_base_ + offset);
        *reg = value;
    }

    int HpetClock::init() {
        const acpi::HPET* hpet = acpi::get_hpet();
        if (!hpet) {
            Log::warning("[HPET] ACPI HPET table not found");
            return -KENOACPI;
        }

        if (hpet->address.address_space != 0) {
            Log::error("[HPET] Unsupported address space id: %u", hpet->address.address_space);
            return -KEINVAL;
        }

        mmio_base_ = reinterpret_cast<volatile u8*>(virt_raw(phys_to_virt(make_phys(hpet->address.address))));

        const u64 gcap = reg_read64(HPET_REG_GCAP_ID);

        // Extract the counter period from bits [63:32].
        period_fs_ = gcap >> HPET_GCAP_PERIOD_SHIFT;

        if (period_fs_ < HPET_MIN_PERIOD_FS || period_fs_ > HPET_MAX_PERIOD_FS) {
            Log::error("[HPET] Counter period out of spec: %llu fs", period_fs_);
            return -KEINVAL;
        }

        // frequency_hz = 1e15 fs/s ÷ period_fs (ticks/fs → Hz)
        freq_hz_   = 1'000'000'000'000'000ULL / period_fs_;
        is_64_bit_  = (gcap & HPET_GCAP_COUNT_SIZE) != 0;

        u64 cfg = reg_read64(HPET_REG_GEN_CONF);
        cfg &= ~HPET_CONF_ENABLE;
        cfg &= ~HPET_CONF_LEG_RT;
        reg_write64(HPET_REG_GEN_CONF, cfg);

        reg_write64(HPET_REG_MAIN_CNT, 0ULL);

        cfg |= HPET_CONF_ENABLE;
        reg_write64(HPET_REG_GEN_CONF, cfg);

        available_ = true;

        Log::ok(
            "[HPET] Initialised: %llu Hz, %u-bit counter, period %llu fs",
            freq_hz_,
            is_64_bit_ ? 64 : 32,
            period_fs_
        );
        return 0;
    }

    u64 HpetClock::read_ticks() {
        u64 raw = reg_read64(HPET_REG_MAIN_CNT);
        // Mask to 32 bits on hardware with a 32-bit counter to prevent phantom rollover.
        if (!is_64_bit_) raw &= 0xFFFF'FFFFULL;
        return raw;
    }

    u64 HpetClock::read_ns() {
        const u64 ticks = read_ticks();
        const u64 ps    = ticks * (period_fs_ / 1'000ULL);
        return ps / 1'000ULL;
    }

} // namespace kernel::time