// tsc_clock.cpp
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

#include "tsc_clock.h"

#include <vespera/kerrno.h>
#include <vespera/log.h>

#include "pit.h"

// Intel® 64 and IA-32 Architectures Software Developer’s Manual Volume 3B: System Programming Guide, Part 2 (17.15)

namespace kernel::time {

    bool TscClock::has_invariant_tsc() {
        // CPUID leaf 0x80000000 tells us the highest supported extended leaf.
        u32 max_ext;
        asm volatile("cpuid" : "=a"(max_ext) : "a"(0x80000000u) : "rbx", "rcx", "rdx");

        if (max_ext < 0x80000007u) {
            return false;
        }

        // Leaf 0x80000007, EDX bit 8 = Invariant TSC.
        u32 edx = 0;
        u32 dummy = 0;
        asm volatile("cpuid" : "=a"(dummy), "=b"(dummy), "=c"(dummy), "=d"(edx) : "a"(0x80000007u) :);
        return (edx & (1u << 8)) != 0;
    }

    u64 TscClock::rdtsc() {
        u32 lo = 0;
        u32 hi = 0;
        asm volatile(
            "lfence\n\t"
            "rdtsc"
            : "=a"(lo), "=d"(hi)
            :
            : "memory"
        );
        return (static_cast<u64>(hi) << 32) | static_cast<u64>(lo);
    }

    u64 TscClock::measure_freq_once() {
        const u64 tsc_before = rdtsc();
        PitClock::busy_wait_us(TSC_CALIBRATE_US);
        const u64 tsc_after = rdtsc();

        const u64 delta = tsc_after - tsc_before;

        if (delta == 0) {
            return 0;
        }

        return (delta * 1'000'000ULL) / TSC_CALIBRATE_US;
    }

    int TscClock::init() {
        if (!has_invariant_tsc()) {
            Log::warning("[TSC ] Invariant TSC not advertised by CPUID - skipping");
            return -KENODEV;
        }

        constexpr int SAMPLES = 7;
        u64 samples[SAMPLES];

        int valid = 0;

        for (int i = 0; i < SAMPLES; i++) {
            const u64 tsc_before = rdtsc();
            PitClock::busy_wait_us(TSC_CALIBRATE_US);
            const u64 tsc_after = rdtsc();

            const u64 delta = tsc_after - tsc_before;

            if (delta == 0) {
                continue;
            }

            samples[valid++] =
                (delta * 1'000'000ULL) / static_cast<u64>(TSC_CALIBRATE_US);
        }

        if (valid < 3) {
            Log::error("[TSC ] Calibration failed - not enough samples");
            return -KEINVAL;
        }

        for (int i = 0; i < valid; i++) {
            for (int j = i + 1; j < valid; j++) {
                if (samples[j] < samples[i]) {
                    const u64 tmp = samples[i];
                    samples[i] = samples[j];
                    samples[j] = tmp;
                }
            }
        }

        freq_hz_ = samples[valid / 2];

        shift_ = 32;
        mult_ = (1'000'000'000ULL << shift_) / freq_hz_;

        tsc_start_ = rdtsc();
        available_ = true;

        Log::ok("[TSC ] Invariant TSC available: %llu Hz (~%llu MHz)", freq_hz_, freq_hz_ / 1'000'000ULL);
        return 0;
    }

    u64 TscClock::read_ticks() {
        return rdtsc();
    }

    u64 TscClock::read_ns() {
        u64 elapsed = rdtsc() - tsc_start_;

        u64 hi;
        u64 lo;

        asm("mulq %3" : "=a"(lo), "=d"(hi) : "a"(elapsed), "r"(mult_));

        return (hi << (64 - shift_)) | (lo >> shift_);
    }

}  // namespace kernel::time