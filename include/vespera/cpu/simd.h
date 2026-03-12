// simd.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 12.03.26.
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
#ifndef VESPERAOS_SIMD_H
#define VESPERAOS_SIMD_H

#include <vespera/types.h>

// ReSharper disable CppInconsistentNaming

/// Bitfield describing which SIMD extensions are both present in hardware
/// AND have been successfully enabled by the OS (CR0/CR4/XCR0).
struct SimdFeatures {
    // x87
    bool fpu       : 1;  ///< x87 on-chip FPU

    // SSE family
    bool sse        : 1;  ///< SSE  – 128-bit XMM float32
    bool sse2       : 1;  ///< SSE2 – XMM integer + float64
    bool sse3       : 1;  ///< SSE3
    bool ssse3      : 1;  ///< Supplemental SSE3
    bool sse4_1     : 1;  ///< SSE4.1
    bool sse4_2     : 1;  ///< SSE4.2

    // AVX family
    bool avx        : 1;  ///< AVX  – 256-bit YMM float
    bool avx2       : 1;  ///< AVX2 – 256-bit YMM integer
    bool fma        : 1;  ///< FMA3 – fused multiply-add (VEX-encoded)

    // AVX-512
    bool avx512f    : 1;  ///< AVX-512 Foundation
    bool avx512bw   : 1;  ///< AVX-512 Byte & Word
    bool avx512dq   : 1;  ///< AVX-512 Doubleword & Quadword
    bool avx512vl   : 1;  ///< AVX-512 Vector Length extensions
    bool avx512cd   : 1;  ///< AVX-512 Conflict Detection
    bool avx512er   : 1;  ///< AVX-512 Exponential & Reciprocal
    bool avx512pf   : 1;  ///< AVX-512 Prefetch
    bool avx512ifma : 1;  ///< AVX-512 Integer FMA
    bool avx512vbmi : 1;  ///< AVX-512 Vector Byte Manipulation Instructions

    bool popcnt     : 1;  ///< POPCNT instruction
    bool aes        : 1;  ///< AES-NI
    bool pclmulqdq  : 1;  ///< Carry-less multiplication
    bool f16c       : 1;  ///< 16-bit float ↔ 32-bit float conversion
    bool bmi1       : 1;  ///< Bit Manipulation Instructions 1
    bool bmi2       : 1;  ///< Bit Manipulation Instructions 2

    u8 rsv0 : 1;
    u32 rsv1;
};

/// Detect, enable, and record all available SIMD extensions.
/// Should only be called on the bsp
void simd_init() noexcept;

// Enable SIMD extensions for AP cores
void simd_enable_on_current_core();

/// All fields are incorrect until simd_init() fills in the fields.
[[nodiscard]] const SimdFeatures& simd_features() noexcept;


[[nodiscard]] inline bool simd_has_avx512() noexcept {
    const auto& f = simd_features();
    return f.avx512f | f.avx512bw | f.avx512dq | f.avx512vl |
           f.avx512cd | f.avx512er | f.avx512pf | f.avx512ifma | f.avx512vbmi;
}

#endif  // VESPERAOS_SIMD_H
