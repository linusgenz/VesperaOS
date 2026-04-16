// clock_manager.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.04.26.
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

#include "clock_manager.h"

#include <vespera/log.h>

#include "apic_clock.h"
#include "hpet.h"
#include "pit.h"

namespace kernel::time::clock_manager {

    namespace {

        HpetClock g_hpet;
        ApicClock g_apic;
        PitClock g_pit;

        IClockSource* g_active = nullptr;

        constexpr IClockSource* const G_SOURCES[] = {
            &g_pit,
            &g_apic,
            &g_hpet,
        };
    }  // namespace

    void init() {
        Log::info("[TIME] Probing clock sources...");

        for (const auto src : G_SOURCES) {
            const int err = src->init();

            if (err < 0) {
                Log::warning("[TIME] %s init failed (err %d)", src->name(), err);
                continue;
            }

            if (!src->available()) continue;

            if (g_active == nullptr || static_cast<u32>(src->priority()) > static_cast<u32>(g_active->priority())) {
                g_active = src;
            }
        }

        if (g_active) {
            Log::ok("[TIME] Active clock source: %s (%llu Hz)", g_active->name(), g_active->frequency_hz());
        } else {
            Log::error("[TIME] No clock source available — time functions will return 0");
        }
    }

    const char* active_source_name() {
        return g_active ? g_active->name() : "none";
    }

    IClockSource* active_source() {
        return g_active;
    }

    u64 read_ticks() {
        return g_active ? g_active->read_ticks() : 0ULL;
    }

    u64 read_ns() {
        return g_active ? g_active->read_ns() : 0ULL;
    }

    u64 read_us() {
        return read_ns() / 1'000ULL;
    }

    u64 read_ms() {
        return read_ns() / 1'000'000ULL;
    }
}  // namespace kernel::time::clock_manager
