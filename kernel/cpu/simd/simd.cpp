// simd.cpp
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

#include <vespera/cpu/simd.h>
#include <vespera/types.h>
#include <klib/string.h>

// Single ABI symbol shared with interrupts.asm (keep offsets in sync!)
struct FpuAbiConfig {
    u32 mode;       // offset 0: FpuMode
    u32 area_size;  // offset 4: bytes needed for FXSAVE/XSAVE
    u64 xsave_mask; // offset 8: XCR0 mask for XSAVE64/XRSTOR64
};

static_assert(offsetof(FpuAbiConfig, mode) == 0);
static_assert(offsetof(FpuAbiConfig, area_size) == 4);
static_assert(offsetof(FpuAbiConfig, xsave_mask) == 8);
static_assert(sizeof(FpuAbiConfig) == 16);

extern "C" FpuAbiConfig g_vespera_fpu_config{
    .mode       = static_cast<u32>(simd::FpuMode::Disabled),
    .area_size  = 512,
    .xsave_mask = 0,
};

namespace {
    struct CpuidResult {
        u32 eax, ebx, ecx, edx;
    };

    constexpr u64 XCR0_X87 = 1ULL << 0;
    constexpr u64 XCR0_SSE = 1ULL << 1;
    constexpr u64 XCR0_YMM = 1ULL << 2;
    constexpr u64 XCR0_OPMASK = 1ULL << 5;
    constexpr u64 XCR0_ZMM_HI = 1ULL << 6;
    constexpr u64 XCR0_HI16 = 1ULL << 7;

    constexpr u64 CR0_MP = 1ULL << 1;
    constexpr u64 CR0_EM = 1ULL << 2;
    constexpr u64 CR0_TS = 1ULL << 3;

    constexpr u64 CR4_OSFXSR = 1ULL << 9;
    constexpr u64 CR4_OSXMMEXCPT = 1ULL << 10;
    constexpr u64 CR4_OSXSAVE = 1ULL << 18;

    constexpr u16 DEFAULT_FCW = 0x037F;
    constexpr u32 DEFAULT_MXCSR = 0x1F80;
    constexpr u64 DEFAULT_XSTATE_BV = XCR0_X87 | XCR0_SSE;

    simd::Features g_features{};

    [[nodiscard]] constexpr bool has_bit(u32 reg, u32 bit) noexcept {
        return ((reg >> bit) & 1u) != 0;
    }

    [[nodiscard]] CpuidResult cpuid(u32 leaf, u32 subleaf = 0) noexcept {
        CpuidResult r{};
        asm volatile("cpuid"
            : "=a"(r.eax), "=b"(r.ebx), "=c"(r.ecx), "=d"(r.edx)
            : "a"(leaf), "c"(subleaf));
        return r;
    }

    [[nodiscard]] u64 xgetbv(u32 xcr) noexcept {
        u32 lo{}, hi{};
        asm volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(xcr));
        return (static_cast<u64>(hi) << 32) | lo;
    }

    void xsetbv(u32 xcr, u64 val) noexcept {
        asm volatile("xsetbv"
            :: "a"(static_cast<u32>(val & 0xFFFFFFFFu)),
            "d"(static_cast<u32>(val >> 32)),
            "c"(xcr)
            : "memory");
    }

    [[nodiscard]] u64 read_cr0() noexcept {
        u64 v{};
        asm volatile("mov %%cr0, %0" : "=r"(v));
        return v;
    }

    void write_cr0(u64 v) noexcept {
        asm volatile("mov %0, %%cr0" :: "r"(v) : "memory");
    }

    [[nodiscard]] u64 read_cr4() noexcept {
        u64 v{};
        asm volatile("mov %%cr4, %0" : "=r"(v));
        return v;
    }

    void write_cr4(u64 v) noexcept {
        asm volatile("mov %0, %%cr4" :: "r"(v) : "memory");
    }

    void enable_sse_cr() noexcept {
        u64 cr0 = read_cr0();
        cr0 = (cr0 & ~(CR0_EM | CR0_TS)) | CR0_MP;
        write_cr0(cr0);

        u64 cr4 = read_cr4();
        cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;
        write_cr4(cr4);
    }

    void enable_xsave_xcr0(bool want_avx, bool want_avx512) noexcept {
        write_cr4(read_cr4() | CR4_OSXSAVE);

        u64 xcr0 = xgetbv(0) | XCR0_X87 | XCR0_SSE;
        if (want_avx) {
            xcr0 |= XCR0_YMM;
        }
        if (want_avx512) {
            xcr0 |= XCR0_OPMASK | XCR0_ZMM_HI | XCR0_HI16;
        }

        xsetbv(0, xcr0);
    }

    void publish_fpu_config(simd::FpuMode mode, u32 area_size, u64 xsave_mask) noexcept {
        g_vespera_fpu_config.area_size = area_size;
        g_vespera_fpu_config.xsave_mask = xsave_mask;
        __atomic_store_n(&g_vespera_fpu_config.mode, static_cast<u32>(mode), __ATOMIC_RELEASE);
    }
} // namespace

namespace simd {
    void enable_on_current_core() noexcept {
        const u32 max_leaf = cpuid(0).eax;
        if (max_leaf < 1) return;

        const auto l1 = cpuid(1);
        const bool cpu_fpu = has_bit(l1.edx, 0);
        const bool cpu_sse = has_bit(l1.edx, 25);
        const bool cpu_sse2 = has_bit(l1.edx, 26);
        const bool cpu_xsave = has_bit(l1.ecx, 26);
        const bool cpu_avx = has_bit(l1.ecx, 28);

        const bool cpu_avx512f = (max_leaf >= 7) && has_bit(cpuid(7, 0).ebx, 16);

        if (cpu_fpu) asm volatile("fninit");
        if (cpu_sse && cpu_sse2) enable_sse_cr();
        if (cpu_xsave) enable_xsave_xcr0(cpu_avx, cpu_avx512f);
    }

    void init() noexcept {
        enable_on_current_core();

        const u32 max_leaf = cpuid(0).eax;
        if (max_leaf < 1) return;

        Features f{};
        const auto l1 = cpuid(1);
        const auto l7 = (max_leaf >= 7) ? cpuid(7, 0) : CpuidResult{};

        const bool cpu_fpu = has_bit(l1.edx, 0);
        const bool cpu_sse = has_bit(l1.edx, 25);
        const bool cpu_sse2 = has_bit(l1.edx, 26);
        const bool cpu_xsave = has_bit(l1.ecx, 26);
        const bool cpu_avx = has_bit(l1.ecx, 28);
        const bool cpu_avx512f = has_bit(l7.ebx, 16);

        f.fpu = cpu_fpu;
        f.sse = cpu_sse && cpu_sse2;
        f.sse2 = f.sse;

        const bool os_xsave_active = cpu_xsave && has_bit(cpuid(1).ecx, 27);

        bool avx_usable = false;
        bool avx512_usable = false;

        if (os_xsave_active && max_leaf >= 0xD) {
            const u64 xcr0 = xgetbv(0);
            const u32 req_size = cpuid(0xD, 0).ebx;

            if (req_size >= 512 && req_size <= MAX_FPU_AREA_SIZE) {
                constexpr u64 avx_mask = XCR0_SSE | XCR0_YMM;
                constexpr u64 avx512_mask = avx_mask | XCR0_OPMASK | XCR0_ZMM_HI | XCR0_HI16;

                avx_usable = cpu_avx && ((xcr0 & avx_mask) == avx_mask);
                avx512_usable = cpu_avx512f && avx_usable && ((xcr0 & avx512_mask) == avx512_mask);

                publish_fpu_config(FpuMode::Xsave, req_size, xcr0);
            } else if (f.sse2) {
                publish_fpu_config(FpuMode::Fxsave, 512, 0);
            }
        } else if (f.sse2) {
            publish_fpu_config(FpuMode::Fxsave, 512, 0);
        }

        f.sse3 = f.sse2 && has_bit(l1.ecx, 0);
        f.ssse3 = f.sse2 && has_bit(l1.ecx, 9);
        f.sse4_1 = f.sse2 && has_bit(l1.ecx, 19);
        f.sse4_2 = f.sse2 && has_bit(l1.ecx, 20);

        f.avx = avx_usable;
        f.avx2 = avx_usable && has_bit(l7.ebx, 5);
        f.fma = avx_usable && has_bit(l1.ecx, 12);
        f.f16c = avx_usable && has_bit(l1.ecx, 29);

        f.avx512f = avx512_usable;
        f.avx512dq = avx512_usable && has_bit(l7.ebx, 17);
        f.avx512ifma = avx512_usable && has_bit(l7.ebx, 21);
        f.avx512pf = avx512_usable && has_bit(l7.ebx, 26);
        f.avx512er = avx512_usable && has_bit(l7.ebx, 27);
        f.avx512cd = avx512_usable && has_bit(l7.ebx, 28);
        f.avx512bw = avx512_usable && has_bit(l7.ebx, 30);
        f.avx512vl = avx512_usable && has_bit(l7.ebx, 31);
        f.avx512vbmi = avx512_usable && has_bit(l7.ecx, 1);

        f.pclmulqdq = has_bit(l1.ecx, 1);
        f.popcnt = has_bit(l1.ecx, 23);
        f.aes = has_bit(l1.ecx, 25);
        f.bmi1 = has_bit(l7.ebx, 3);
        f.bmi2 = has_bit(l7.ebx, 8);

        g_features = f;
    }

    const Features& features() noexcept {
        return g_features;
    }

    FpuMode fpu_mode() noexcept {
        return static_cast<FpuMode>(__atomic_load_n(&g_vespera_fpu_config.mode, __ATOMIC_ACQUIRE));
    }

    u32 fpu_area_size() noexcept {
        return g_vespera_fpu_config.area_size;
    }

    void save_state(FpuState& state) noexcept {
        switch (fpu_mode()) {
            case FpuMode::Xsave: {
                __builtin_memset(state.area + 512, 0, 64);
                const u64 mask = g_vespera_fpu_config.xsave_mask;
                const u32 lo = static_cast<u32>(mask & 0xFFFFFFFFu);
                const u32 hi = static_cast<u32>(mask >> 32);
                asm volatile("xsave64 %0" : "+m"(state.area) : "a"(lo), "d"(hi) : "memory");
                break;
            }
            case FpuMode::Fxsave:
                asm volatile("fxsave64 %0" : "=m"(state.area) :: "memory");
                break;
            case FpuMode::Disabled:
                break;
        }
    }

    void restore_state(const FpuState& state) noexcept {
        switch (fpu_mode()) {
            case FpuMode::Xsave: {
                const u64 mask = g_vespera_fpu_config.xsave_mask;
                const u32 lo = static_cast<u32>(mask & 0xFFFFFFFFu);
                const u32 hi = static_cast<u32>(mask >> 32);
                asm volatile("xrstor64 %0" :: "m"(state.area), "a"(lo), "d"(hi) : "memory");
                break;
            }
            case FpuMode::Fxsave:
                asm volatile("fxrstor64 %0" :: "m"(state.area) : "memory");
                break;
            case FpuMode::Disabled:
                break;
        }
    }

    void init_state(FpuState& state) noexcept {
        memset(state.area, 0, sizeof(state.area));
        memcpy(state.area + 0, &DEFAULT_FCW, sizeof(DEFAULT_FCW));
        memcpy(state.area + 24, &DEFAULT_MXCSR, sizeof(DEFAULT_MXCSR));
        memcpy(state.area + 512, &DEFAULT_XSTATE_BV, sizeof(DEFAULT_XSTATE_BV));
    }
} // namespace simd
