// simd_mem.h
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
#ifndef VESPERAOS_SIMD_MEM_H
#define VESPERAOS_SIMD_MEM_H

#include <vespera/types.h>

extern "C" {
    void* simd_memcpy_avx2 (void* dst, const void* src, usize len);
    void* simd_memcpy_sse2 (void* dst, const void* src, usize len);
    void* simd_memmove_avx2(void* dst, const void* src, usize len);
    void  simd_memset_avx2 (void* dst, u32 value, usize len);
    void  simd_memset_sse2 (void* dst, u32 value, usize len);

    void* fb_memcpy_avx2(void* dst, const void* src, usize len);
    void  fb_memset_avx2(void* dst, u32 value, usize len);
    void* fb_memmove_avx2(void* dst, const void* src, usize len);
    void* fb_memcpy_sse2(void* dst, const void* src, usize len);
    void  fb_memset_sse2(void* dst, u32 value, usize len);
}


#endif  // VESPERAOS_SIMD_MEM_H
