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

// ---------------------------
// Atomic uint8_t
// ---------------------------
typedef struct atomic_u8 {
    volatile uint8_t value{};

    void init(uint8_t v = 0) {
        value = v;
    }

    void store(uint8_t v) {
        asm volatile("xchg %0, %1" : "+r"(v), "+m"(value) :: "memory");
    }

    [[nodiscard]] uint8_t load() const {
        uint8_t v;
        asm volatile("movb %1, %0" : "=r"(v) : "m"(value) : "memory");
        return v;
    }

    uint8_t fetch_add(uint8_t inc) {
        uint8_t old;
        asm volatile("lock xaddb %0, %1"
                         : "=r"(old), "+m"(value)
                         : "0"(inc)
                         : "memory");
        return old;
    }

    uint8_t fetch_sub(uint8_t dec) {
        return fetch_add(static_cast<uint8_t>(-dec));
    }

    bool compare_exchange(uint8_t *expected, uint8_t desired) {
        uint8_t old = *expected;
        asm volatile("lock cmpxchgb %2, %1"
                         : "=a"(old), "+m"(value)
                         : "r"(desired), "0"(old)
                         : "memory");
        *expected = old;
        return old == *expected;
    }

    atomic_u8& operator++() {
        fetch_add(1);
        return *this;
    }

    uint8_t operator++(int) {
        return fetch_add(1);
    }
} atomic_u8_t;


// ---------------------------
// Atomic uint16_t
// ---------------------------
typedef struct atomic_u16 {
    volatile uint16_t value{};

    void init(uint16_t v = 0) {
        value = v;
    }

    void store(uint16_t v) {
        asm volatile("xchg %0, %1" : "+r"(v), "+m"(value) :: "memory");
    }

    [[nodiscard]] uint16_t load() const {
        uint16_t v;
        asm volatile("movw %1, %0" : "=r"(v) : "m"(value) : "memory");
        return v;
    }

    uint16_t fetch_add(uint16_t inc) {
        uint16_t old;
        asm volatile("lock xaddw %0, %1"
                         : "=r"(old), "+m"(value)
                         : "0"(inc)
                         : "memory");
        return old;
    }

    uint16_t fetch_sub(uint16_t dec) {
        return fetch_add(static_cast<uint16_t>(-dec));
    }

    bool compare_exchange(uint16_t *expected, uint16_t desired) {
        uint16_t old = *expected;
        asm volatile("lock cmpxchgw %2, %1"
                         : "=a"(old), "+m"(value)
                         : "r"(desired), "0"(old)
                         : "memory");
        *expected = old;
        return old == *expected;
    }

    atomic_u16& operator++() {
        fetch_add(1);
        return *this;
    }

    uint16_t operator++(int) {
        return fetch_add(1);
    }
} atomic_u16_t;

// ---------------------------
// Atomic uint32_t
// ---------------------------
typedef struct atomic_u32 {
    volatile uint32_t value{};

    void init(uint32_t v = 0) {
        value = v;
    }

    void store(uint32_t v) {
        asm volatile("xchg %0, %1" : "+r"(v), "+m"(value) :: "memory");
    }

    [[nodiscard]] uint32_t load() const {
        uint32_t v;
        asm volatile("movl %1, %0" : "=r"(v) : "m"(value) : "memory");
        return v;
    }

    uint32_t fetch_add(uint32_t inc) {
        uint32_t old;
        asm volatile("lock xaddl %0, %1"
                         : "=r"(old), "+m"(value)
                         : "0"(inc)
                         : "memory");
        return old;
    }

    uint32_t fetch_sub(uint32_t dec) {
        return fetch_add(-dec);
    }

    bool compare_exchange(uint32_t *expected, uint32_t desired) {
        uint32_t old = *expected;
        asm volatile("lock cmpxchgl %2, %1"
                         : "=a"(old), "+m"(value)
                         : "r"(desired), "0"(old)
                         : "memory");
        *expected = old;
        return old == *expected;
    }

    atomic_u32& operator++() {
        fetch_add(1);
        return *this;
    }

    uint32_t operator++(int) {
        return fetch_add(1);
    }
} atomic_u32_t;


// ---------------------------
// Atomic uint64_t
// ---------------------------
typedef struct atomic_u64 {
    volatile uint64_t value{};

    void init(uint64_t v = 0) {
        value = v;
    }

    void store(uint64_t v) {
        asm volatile("xchg %0, %1" : "+r"(v), "+m"(value) :: "memory");
    }

    [[nodiscard]] uint64_t load() const {
        uint64_t v;
        asm volatile("movq %1, %0" : "=r"(v) : "m"(value) : "memory");
        return v;
    }

    uint64_t fetch_add(uint64_t inc) {
        uint64_t old;
        asm volatile("lock xaddq %0, %1"
                         : "=r"(old), "+m"(value)
                         : "0"(inc)
                         : "memory");
        return old;
    }

    uint64_t fetch_sub(uint64_t dec) {
        return fetch_add(-dec);
    }

    bool compare_exchange(uint64_t *expected, uint64_t desired) {
        uint64_t old = *expected;
        asm volatile("lock cmpxchgq %2, %1"
                         : "=a"(old), "+m"(value)
                         : "r"(desired), "0"(old)
                         : "memory");
        *expected = old;
        return old == *expected;
    }

    atomic_u64& operator++() {
        fetch_add(1);
        return *this;
    }

    uint64_t operator++(int) {
        return fetch_add(1);
    }
} atomic_u64_t;

// ---------------------------
// Atomic flag
// ---------------------------

typedef struct atomic_flag {
    volatile uint8_t value{};

    void init(bool v = false) {
        value = v ? 1 : 0;
    }

    bool test_and_set() {
        uint8_t old = 1;
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
        uint8_t v = 0;
        asm volatile("xchg %0, %1"
                     : "+r"(v), "+m"(value)
                     :
                     : "memory");
    }

    [[nodiscard]] bool load() const {
        uint8_t v;
        asm volatile("movb %1, %0"
                     : "=r"(v)
                     : "m"(value)
                     : "memory");
        return v != 0;
    }

    bool compare_exchange(bool *expected, bool desired) {
        uint8_t exp = *expected ? 1 : 0;
        uint8_t des = desired ? 1 : 0;
        uint8_t old = exp;
        asm volatile("lock cmpxchgb %2, %1"
                     : "=a"(old), "+m"(value)
                     : "r"(des), "0"(old)
                     : "memory");
        *expected = (old != 0);
        return old == exp;
    }
} atomic_flag_t;

#endif //VESPERAOS_ATOMIC_H