// cxx_runtime.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 05.06.26.
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

#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

void* memcpy(void* dest, const void* src, size_t len) {
    char* d = (char*)dest;
    const char* s = (char*)src;
    while (len--) *d++ = *s++;
    return dest;
}

int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

char* strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    if (*needle == '\0') return (char*)haystack;

    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (*n == '\0') return (char*)haystack;
    }
    return NULL;
}

extern "C" void* malloc(size_t size);
extern "C" void free(void* ptr);

void* operator new(size_t size) {
    return malloc(size);
}

void* operator new[](size_t size) {
    return malloc(size);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete[](void* ptr) noexcept {
    free(ptr);
}

void operator delete(void* ptr, size_t size) noexcept {
    free(ptr);
}

void operator delete[](void* ptr, size_t size) noexcept {
    free(ptr);
}

namespace std {
    struct nothrow_t {
    };

    extern const nothrow_t nothrow;
}

void* operator new(size_t size, const std::nothrow_t&) noexcept {
    return malloc(size);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    return malloc(size);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    free(ptr);
}

extern "C" {
void __cxa_pure_virtual() {
    abort();
}
// TODO MULTITHREAD SAVE PLS
static pthread_mutex_t cxa_guard_mutex = PTHREAD_MUTEX_INITIALIZER;

extern "C" int __cxa_guard_acquire(uint64_t* guard_object) {
    char* initialized = reinterpret_cast<char*>(guard_object);

    if (__atomic_load_n(initialized, __ATOMIC_ACQUIRE) != 0) {
        return 0;
    }

    pthread_mutex_lock(&cxa_guard_mutex);

    if (*initialized != 0) {
        pthread_mutex_unlock(&cxa_guard_mutex);
        return 0;
    }

    return 1;
}

extern "C" void __cxa_guard_release(uint64_t* guard_object) {
    char* initialized = reinterpret_cast<char*>(guard_object);

    __atomic_store_n(initialized, 1, __ATOMIC_RELEASE);

    pthread_mutex_unlock(&cxa_guard_mutex);
}

extern "C" void __cxa_guard_abort(uint64_t* guard_object) {
    pthread_mutex_unlock(&cxa_guard_mutex);
}
}

namespace std {
    void terminate() noexcept {
        abort();
    }
}
