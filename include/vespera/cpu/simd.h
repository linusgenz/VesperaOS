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

namespace simd {

    inline constexpr usize MAX_FPU_AREA_SIZE = 3072; // 0xC00, matches FPU_AREA_SIZE in interrupts.asm

    enum class FpuMode : u32 {
        Disabled = 0,
        Fxsave   = 1,
        Xsave    = 2,
    };

    struct alignas(64) FpuState {
        static constexpr usize SIZE = MAX_FPU_AREA_SIZE;
        u8 area[SIZE];
    };

    struct Features {
        bool fpu        : 1 = false;

        bool sse        : 1 = false;
        bool sse2       : 1 = false;
        bool sse3       : 1 = false;
        bool ssse3      : 1 = false;
        bool sse4_1     : 1 = false;
        bool sse4_2     : 1 = false;

        bool avx        : 1 = false;
        bool avx2       : 1 = false;
        bool fma        : 1 = false;

        bool avx512f    : 1 = false;
        bool avx512bw   : 1 = false;
        bool avx512dq   : 1 = false;
        bool avx512vl   : 1 = false;
        bool avx512cd   : 1 = false;
        bool avx512er   : 1 = false;
        bool avx512pf   : 1 = false;
        bool avx512ifma : 1 = false;
        bool avx512vbmi : 1 = false;

        bool popcnt     : 1 = false;
        bool aes        : 1 = false;
        bool pclmulqdq  : 1 = false;
        bool f16c       : 1 = false;
        bool bmi1       : 1 = false;
        bool bmi2       : 1 = false;
    };

    /// Detect, enable, and record all available SIMD extensions (BSP only).
    void init() noexcept;

    /// Enable SIMD extensions on the current core (BSP & APs).
    void enable_on_current_core() noexcept;

    [[nodiscard]] const Features& features() noexcept;
    [[nodiscard]] FpuMode fpu_mode() noexcept;
    [[nodiscard]] u32 fpu_area_size() noexcept;

    [[nodiscard]] inline bool has_avx512() noexcept {
        return features().avx512f;
    }

    void save_state(FpuState& state) noexcept;
    void restore_state(const FpuState& state) noexcept;
    void init_state(FpuState& state) noexcept;

} // namespace simd

#endif // VESPERAOS_SIMD_H
