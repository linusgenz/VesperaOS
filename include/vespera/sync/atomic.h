// atomic.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 18.11.25.
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

#ifndef VESPERAOS_ATOMIC_H
#define VESPERAOS_ATOMIC_H
#include <vespera/types.h>

// ---------------------------
// Atomic u8
// ---------------------------
typedef struct AtomicU8 {
    volatile u8 value{};

    void init(u8 v = 0) {
        value = v;
    }

    void store(u8 v) {
        asm volatile("xchg %0, %1" : "+r"(v), "+m"(value) :: "memory");
    }

    [[nodiscard]] u8 load() const {
        u8 v;
        asm volatile("movb %1, %0" : "=r"(v) : "m"(value) : "memory");
        return v;
    }

    u8 fetch_add(u8 inc) {
        u8 old;
        asm volatile("lock xaddb %0, %1"
                         : "=r"(old), "+m"(value)
                         : "0"(inc)
                         : "memory");
        return old;
    }

    u8 fetch_sub(u8 dec) {
        return fetch_add(static_cast<u8>(-dec));
    }

    bool compare_exchange(u8 *expected, u8 desired) {
        u8 old = *expected;
        asm volatile("lock cmpxchgb %2, %1"
                         : "=a"(old), "+m"(value)
                         : "r"(desired), "0"(old)
                         : "memory");
        *expected = old;
        return old == *expected;
    }

    AtomicU8& operator++() {
        fetch_add(1);
        return *this;
    }

    u8 operator++(int) {
        return fetch_add(1);
    }
} atomic_u8_t;


// ---------------------------
// Atomic u16
// ---------------------------
typedef struct AtomicU16 {
    volatile u16 value{};

    void init(u16 v = 0) {
        value = v;
    }

    void store(u16 v) {
        asm volatile("xchg %0, %1" : "+r"(v), "+m"(value) :: "memory");
    }

    [[nodiscard]] u16 load() const {
        u16 v;
        asm volatile("movw %1, %0" : "=r"(v) : "m"(value) : "memory");
        return v;
    }

    u16 fetch_add(u16 inc) {
        u16 old;
        asm volatile("lock xaddw %0, %1"
                         : "=r"(old), "+m"(value)
                         : "0"(inc)
                         : "memory");
        return old;
    }

    u16 fetch_sub(u16 dec) {
        return fetch_add(static_cast<u16>(-dec));
    }

    bool compare_exchange(u16 *expected, u16 desired) {
        u16 old = *expected;
        asm volatile("lock cmpxchgw %2, %1"
                         : "=a"(old), "+m"(value)
                         : "r"(desired), "0"(old)
                         : "memory");
        *expected = old;
        return old == *expected;
    }

    AtomicU16& operator++() {
        fetch_add(1);
        return *this;
    }

    u16 operator++(int) {
        return fetch_add(1);
    }
} atomic_u16_t;

// ---------------------------
// Atomic u32
// ---------------------------
typedef struct AtomicU32 {
    volatile u32 value{};

    void init(u32 v = 0) {
        value = v;
    }

    void store(u32 v) {
        asm volatile("xchg %0, %1" : "+r"(v), "+m"(value) :: "memory");
    }

    [[nodiscard]] u32 load() const {
        u32 v;
        asm volatile("movl %1, %0" : "=r"(v) : "m"(value) : "memory");
        return v;
    }

    u32 fetch_add(u32 inc) {
        u32 old;
        asm volatile("lock xaddl %0, %1"
                         : "=r"(old), "+m"(value)
                         : "0"(inc)
                         : "memory");
        return old;
    }

    u32 fetch_sub(u32 dec) {
        return fetch_add(-dec);
    }

    bool compare_exchange(u32 *expected, u32 desired) {
        u32 old = *expected;
        asm volatile("lock cmpxchgl %2, %1"
                         : "=a"(old), "+m"(value)
                         : "r"(desired), "0"(old)
                         : "memory");
        *expected = old;
        return old == *expected;
    }

    AtomicU32& operator++() {
        fetch_add(1);
        return *this;
    }

    u32 operator++(int) {
        return fetch_add(1);
    }
} atomic_u32_t;


// ---------------------------
// Atomic u64
// ---------------------------
typedef struct AtomicU64 {
    volatile u64 value{};

    void init(u64 v = 0) {
        value = v;
    }

    void store(u64 v) {
        asm volatile("xchg %0, %1" : "+r"(v), "+m"(value) :: "memory");
    }

    [[nodiscard]] u64 load() const {
        u64 v;
        asm volatile("movq %1, %0" : "=r"(v) : "m"(value) : "memory");
        return v;
    }

    u64 fetch_add(u64 inc) {
        u64 old;
        asm volatile("lock xaddq %0, %1"
                         : "=r"(old), "+m"(value)
                         : "0"(inc)
                         : "memory");
        return old;
    }

    u64 fetch_sub(u64 dec) {
        return fetch_add(-dec);
    }

    bool compare_exchange(u64 *expected, u64 desired) {
        u64 old = *expected;
        asm volatile("lock cmpxchgq %2, %1"
                         : "=a"(old), "+m"(value)
                         : "r"(desired), "0"(old)
                         : "memory");
        *expected = old;
        return old == *expected;
    }

    AtomicU64& operator++() {
        fetch_add(1);
        return *this;
    }

    u64 operator++(int) {
        return fetch_add(1);
    }
} atomic_u64_t;

// ---------------------------
// Atomic flag
// ---------------------------

typedef struct AtomicFlag {
    volatile u8 value{};

    void init(bool v = false) {
        value = v ? 1 : 0;
    }

    bool test_and_set() {
        u8 old = 1;
        asm volatile("xchg %0, %1"
                     : "+r"(old), "+m"(value)
                     :
                     : "memory");
        return old != 0;
    }

    void set(bool v = true) {
        asm volatile("xchg %0, %1" : "+r"(v), "+m"(value) :: "memory");
    }

    void clear() {
        u8 v = 0;
        asm volatile("xchg %0, %1"
                     : "+r"(v), "+m"(value)
                     :
                     : "memory");
    }

    [[nodiscard]] bool load() const {
        u8 v;
        asm volatile("movb %1, %0"
                     : "=r"(v)
                     : "m"(value)
                     : "memory");
        return v != 0;
    }

    bool compare_exchange(bool *expected, bool desired) {
        u8 exp = *expected ? 1 : 0;
        u8 des = desired ? 1 : 0;
        u8 old = exp;
        asm volatile("lock cmpxchgb %2, %1"
                     : "=a"(old), "+m"(value)
                     : "r"(des), "0"(old)
                     : "memory");
        *expected = (old != 0);
        return old == exp;
    }
} atomic_flag_t;

#endif //VESPERAOS_ATOMIC_H