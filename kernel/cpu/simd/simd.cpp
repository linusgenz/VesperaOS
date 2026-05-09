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

#include <vespera/types.h>
#include <vespera/cpu/simd.h>

struct CpuidResult {
    u32 eax, ebx, ecx, edx;
};

[[nodiscard]] static inline CpuidResult cpuid(u32 leaf, u32 subleaf = 0) noexcept {
    CpuidResult r{};
    asm volatile (
        "cpuid"
        : "=a"(r.eax), "=b"(r.ebx), "=c"(r.ecx), "=d"(r.edx)
        : "a"(leaf), "c"(subleaf)
    );
    return r;
}

[[nodiscard]] static inline u64 xgetbv(u32 xcr) noexcept {
    u32 lo, hi;
    asm volatile (
        "xgetbv"
        : "=a"(lo), "=d"(hi)
        : "c"(xcr)
    );
    return (static_cast<u64>(hi) << 32) | lo;
}

[[nodiscard]] static inline u64 read_cr0() noexcept {
    u64 v;
    asm volatile ("mov %%cr0, %0" : "=r"(v));
    return v;
}

static inline void write_cr0(u64 v) noexcept {
    asm volatile ("mov %0, %%cr0" :: "r"(v) : "memory");
}

[[nodiscard]] static inline u64 read_cr4() noexcept {
    u64 v;
    asm volatile ("mov %%cr4, %0" : "=r"(v));
    return v;
}

static inline void write_cr4(u64 v) noexcept {
    asm volatile ("mov %0, %%cr4" :: "r"(v) : "memory");
}

SimdFeatures g_simd_features{};

static constexpr u64 XCR0_X87   = (1ULL << 0);
static constexpr u64 XCR0_SSE   = (1ULL << 1);  // XMM state
static constexpr u64 XCR0_YMM   = (1ULL << 2);  // YMM upper halves
static constexpr u64 XCR0_OPMASK= (1ULL << 5);  // AVX-512 opmask regs
static constexpr u64 XCR0_ZMM_HI= (1ULL << 6);  // ZMM0-15 upper bits
static constexpr u64 XCR0_HI16  = (1ULL << 7);  // ZMM16-31

static constexpr u64 CR0_EM  = (1ULL << 2);  // Emulation – must be 0 for SSE
static constexpr u64 CR0_MP  = (1ULL << 1);  // Monitor co-processor
static constexpr u64 CR0_TS  = (1ULL << 3);  // Task-switched (lazy FPU; clear for init)

static constexpr u64 CR4_OSFXSR   = (1ULL << 9);   // OS supports FXSAVE/FXRSTOR
static constexpr u64 CR4_OSXMMEXCPT= (1ULL << 10); // OS handles SIMD FP exceptions
static constexpr u64 CR4_OSXSAVE  = (1ULL << 18);  // OS enables XSAVE/XRSTOR

// Enable SSE in CR0 / CR4
static void enable_sse_cr() noexcept {
    // Clear EM, set MP in CR0
    u64 cr0 = read_cr0();
    cr0 &= ~CR0_EM;
    cr0 |=  CR0_MP;
    cr0 &= ~CR0_TS;
    write_cr0(cr0);

    // Set OSFXSR + OSXMMEXCPT in CR4
    u64 cr4 = read_cr4();
    cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;
    write_cr4(cr4);
}

// Enable XSAVE in CR4 and set XCR0 to expose YMM / ZMM state
static void enable_xsave_xcr0(bool want_avx, bool want_avx512) noexcept {
    u64 cr4 = read_cr4();
    cr4 |= CR4_OSXSAVE;
    write_cr4(cr4);

    u64 xcr0 = xgetbv(0);
    xcr0 |= XCR0_X87 | XCR0_SSE;

    if (want_avx) {
        xcr0 |= XCR0_YMM;
    }
    if (want_avx512) {
        xcr0 |= XCR0_OPMASK | XCR0_ZMM_HI | XCR0_HI16;
    }

    asm volatile (
        "xsetbv"
        :: "a"(static_cast<u32>(xcr0 & 0xFFFFFFFF)),
           "d"(static_cast<u32>(xcr0 >> 32)),
           "c"(0u)
        : "memory"
    );
}

void simd_enable_on_current_core() {
    auto l1 = cpuid(1);
    bool cpu_fpu   = (l1.edx >> 0)  & 1;
    bool cpu_sse   = (l1.edx >> 25) & 1;
    bool cpu_sse2  = (l1.edx >> 26) & 1;
    bool cpu_xsave = (l1.ecx >> 26) & 1;
    bool cpu_avx   = (l1.ecx >> 28) & 1;
    auto l7        = cpuid(7, 0);
    bool cpu_avx512f = (l7.ebx >> 16) & 1;

    if (cpu_fpu)            asm volatile("fninit");
    if (cpu_sse && cpu_sse2) enable_sse_cr();
    if (cpu_xsave)          enable_xsave_xcr0(cpu_avx, cpu_avx512f);
}


void simd_init() noexcept {
    simd_enable_on_current_core();

    SimdFeatures f{};

    // CPUID leaf 1 – standard features
    auto l1 = cpuid(1);

    // ECX bits
    bool cpu_sse3      = (l1.ecx >> 0)  & 1;
    bool cpu_pclmulqdq = (l1.ecx >> 1)  & 1;
    bool cpu_ssse3     = (l1.ecx >> 9)  & 1;
    bool cpu_fma       = (l1.ecx >> 12) & 1;
    bool cpu_sse4_1    = (l1.ecx >> 19) & 1;
    bool cpu_sse4_2    = (l1.ecx >> 20) & 1;
    bool cpu_aes       = (l1.ecx >> 25) & 1;
    bool cpu_xsave     = (l1.ecx >> 26) & 1;
    bool cpu_avx       = (l1.ecx >> 28) & 1;
    bool cpu_f16c      = (l1.ecx >> 29) & 1;

    // EDX bits
    bool cpu_fpu  = (l1.edx >> 0)  & 1;
    bool cpu_sse  = (l1.edx >> 25) & 1;
    bool cpu_sse2 = (l1.edx >> 26) & 1;

    bool cpu_popcnt = (l1.ecx >> 23) & 1;

    // CPUID leaf 7, subleaf 0 – extended features
    auto l7 = cpuid(7, 0);

    bool cpu_avx2      = (l7.ebx >> 5)  & 1;
    bool cpu_bmi1      = (l7.ebx >> 3)  & 1;
    bool cpu_bmi2      = (l7.ebx >> 8)  & 1;
    bool cpu_avx512f   = (l7.ebx >> 16) & 1;
    bool cpu_avx512dq  = (l7.ebx >> 17) & 1;
    bool cpu_avx512ifma= (l7.ebx >> 21) & 1;
    bool cpu_avx512pf  = (l7.ebx >> 26) & 1;
    bool cpu_avx512er  = (l7.ebx >> 27) & 1;
    bool cpu_avx512cd  = (l7.ebx >> 28) & 1;
    bool cpu_avx512bw  = (l7.ebx >> 30) & 1;
    bool cpu_avx512vl  = (l7.ebx >> 31) & 1;
    bool cpu_avx512vbmi= (l7.ecx >> 1)  & 1;

    // Enable x87 FPU + SSE (always if the CPU has them)
    if (cpu_fpu) {
        f.fpu = true;
    }

    if (cpu_sse && cpu_sse2) {
        f.sse  = true;
        f.sse2 = true;
    }

    // Enable XSAVE + extend XCR0 if the CPU & OS path support it
    bool os_xsave_active = false;

    if (cpu_xsave) {
        auto verify = cpuid(1);
        os_xsave_active = (verify.ecx >> 27) & 1;
    }

    bool avx_usable     = false;
    bool avx512_usable  = false;

    if (os_xsave_active) {
        u64 xcr0 = xgetbv(0);

        // AVX requires XMM + YMM state saved by OS
        avx_usable = cpu_avx &&
                     ((xcr0 & (XCR0_SSE | XCR0_YMM)) == (XCR0_SSE | XCR0_YMM));

        // AVX-512 additionally requires opmask + ZMM state
        constexpr u64 avx512_xcr0_mask = XCR0_SSE | XCR0_YMM |
                                         XCR0_OPMASK | XCR0_ZMM_HI | XCR0_HI16;
        avx512_usable = cpu_avx512f && avx_usable &&
                        ((xcr0 & avx512_xcr0_mask) == avx512_xcr0_mask);
    }

    // SSE extensions
    f.sse3      = f.sse2 && cpu_sse3;
    f.ssse3     = f.sse2 && cpu_ssse3;
    f.sse4_1    = f.sse2 && cpu_sse4_1;
    f.sse4_2    = f.sse2 && cpu_sse4_2;

    // AVX
    f.avx  = avx_usable;
    f.avx2 = avx_usable && cpu_avx2;
    f.fma  = avx_usable && cpu_fma;
    f.f16c = avx_usable && cpu_f16c;

    // AVX-512
    f.avx512f    = avx512_usable;
    f.avx512bw   = avx512_usable && cpu_avx512bw;
    f.avx512dq   = avx512_usable && cpu_avx512dq;
    f.avx512vl   = avx512_usable && cpu_avx512vl;
    f.avx512cd   = avx512_usable && cpu_avx512cd;
    f.avx512er   = avx512_usable && cpu_avx512er;
    f.avx512pf   = avx512_usable && cpu_avx512pf;
    f.avx512ifma = avx512_usable && cpu_avx512ifma;
    f.avx512vbmi = avx512_usable && cpu_avx512vbmi;

    f.popcnt    = cpu_popcnt;
    f.aes       = cpu_aes;
    f.pclmulqdq = cpu_pclmulqdq;
    f.bmi1      = cpu_bmi1;
    f.bmi2      = cpu_bmi2;

    g_simd_features = f;
}

[[nodiscard]] const SimdFeatures& simd_features() noexcept {
    return g_simd_features;
}