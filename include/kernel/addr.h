// addr.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 03.03.26.
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

#ifndef VESPERAOS_ADDR_H
#define VESPERAOS_ADDR_H

#include <cstdint>

// ============================================================================
// Physical Address
// ============================================================================

typedef struct { uint64_t raw; } phys_addr_t;

inline phys_addr_t make_phys(uint64_t raw)       { return {raw}; }
inline uint64_t    phys_raw (phys_addr_t a)       { return a.raw; }

inline phys_addr_t phys_add (phys_addr_t a, uint64_t offset) { return {a.raw + offset}; }
inline phys_addr_t phys_sub (phys_addr_t a, uint64_t offset) { return {a.raw - offset}; }
inline uint64_t    phys_diff(phys_addr_t a, phys_addr_t b)   { return a.raw - b.raw;    }

inline bool phys_eq (phys_addr_t a, phys_addr_t b) { return a.raw == b.raw; }
inline bool phys_lt (phys_addr_t a, phys_addr_t b) { return a.raw <  b.raw; }
inline bool phys_gt (phys_addr_t a, phys_addr_t b) { return a.raw >  b.raw; }
inline bool phys_null(phys_addr_t a)                { return a.raw == 0;     }

inline bool phys_is_aligned(phys_addr_t a, uint64_t align) {
    return (a.raw & (align - 1)) == 0;
}
inline phys_addr_t phys_align_up(phys_addr_t a, uint64_t align) {
    return {(a.raw + align - 1) & ~(align - 1)};
}
inline phys_addr_t phys_align_down(phys_addr_t a, uint64_t align) {
    return {a.raw & ~(align - 1)};
}

// ============================================================================
// Virtual Address
// ============================================================================

typedef struct { void* ptr; } virt_addr_t;

inline virt_addr_t make_virt(void* ptr)      { return {ptr}; }
constexpr virt_addr_t virt_from_raw(uint64_t r) {
    return {__builtin_bit_cast(void*, r)};
}
inline uint64_t    virt_raw(virt_addr_t a)   { return reinterpret_cast<uint64_t>(a.ptr); }
inline void*       virt_ptr(virt_addr_t a)   { return a.ptr; }

template<typename T>
inline T* virt_as(virt_addr_t a) {
    return static_cast<T*>(a.ptr);
}

inline virt_addr_t virt_add(virt_addr_t a, uint64_t offset) {
    return {static_cast<uint8_t*>(a.ptr) + offset};
}
inline virt_addr_t virt_sub(virt_addr_t a, uint64_t offset) {
    return {static_cast<uint8_t*>(a.ptr) - offset};
}
inline uint64_t virt_diff(virt_addr_t a, virt_addr_t b) {
    return static_cast<uint8_t*>(a.ptr) - static_cast<uint8_t*>(b.ptr);
}

inline bool virt_eq  (virt_addr_t a, virt_addr_t b) { return a.ptr == b.ptr; }
inline bool virt_lt  (virt_addr_t a, virt_addr_t b) { return a.ptr <  b.ptr; }
inline bool virt_null(virt_addr_t a)                 { return a.ptr == nullptr; }

inline bool virt_is_aligned(virt_addr_t a, uint64_t align) {
    return (virt_raw(a) & (align - 1)) == 0;
}
inline virt_addr_t virt_align_up(virt_addr_t a, uint64_t align) {
    return virt_from_raw((virt_raw(a) + align - 1) & ~(align - 1));
}
inline virt_addr_t virt_align_down(virt_addr_t a, uint64_t align) {
    return virt_from_raw(virt_raw(a) & ~(align - 1));
}

// ============================================================================
// GPU / GGTT Address
// ============================================================================

typedef struct { uint64_t raw; } gfx_addr_t;

inline gfx_addr_t make_gfx(uint64_t raw)      { return {raw}; }
inline uint64_t   gfx_raw  (gfx_addr_t a)     { return a.raw; }

inline gfx_addr_t gfx_add (gfx_addr_t a, uint64_t offset) { return {a.raw + offset}; }
inline gfx_addr_t gfx_sub (gfx_addr_t a, uint64_t offset) { return {a.raw - offset}; }
inline uint64_t   gfx_diff(gfx_addr_t a, gfx_addr_t b)    { return a.raw - b.raw;    }

inline bool gfx_eq  (gfx_addr_t a, gfx_addr_t b) { return a.raw == b.raw; }
inline bool gfx_null(gfx_addr_t a)                { return a.raw == 0;     }

inline bool gfx_is_aligned(gfx_addr_t a, uint64_t align) {
    return (a.raw & (align - 1)) == 0;
}
inline gfx_addr_t gfx_align_up(gfx_addr_t a, uint64_t align) {
    return {(a.raw + align - 1) & ~(align - 1)};
}
inline gfx_addr_t gfx_align_down(gfx_addr_t a, uint64_t align) {
    return {a.raw & ~(align - 1)};
}

static_assert(sizeof(phys_addr_t) == sizeof(uint64_t));
static_assert(sizeof(virt_addr_t) == sizeof(void*));
static_assert(sizeof(gfx_addr_t)  == sizeof(uint64_t));

#endif  // VESPERAOS_ADDR_H
