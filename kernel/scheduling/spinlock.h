//
// Created by Linus on 17.07.25.
//

#ifndef SPINLOCK_H
#define SPINLOCK_H
#include "stdint.h"

struct spinlock_t {
    volatile uint32_t locked;

    void init() {
        locked = 0;
    }

    void lock() {
        while (xchg(&locked, 1)) {
            asm volatile("pause");
        }
    }

    void unlock() {
        locked = 0;
    }

private:
    uint32_t xchg(volatile uint32_t* ptr, uint32_t val) {
        uint32_t old;
        __asm__ volatile (
            "lock xchg %0, %1"
            : "=r"(old), "+m"(*ptr)
            : "0"(val)
            : "memory"
        );
        return old;
    }
};

struct spinlock_guard {
    spinlock_t& lock_ref;

    explicit spinlock_guard(spinlock_t& lock) : lock_ref(lock) {
        lock_ref.lock();
    }

    ~spinlock_guard() {
        lock_ref.unlock();
    }

    spinlock_guard(const spinlock_guard&) = delete;
    spinlock_guard& operator=(const spinlock_guard&) = delete;
};


#endif //SPINLOCK_H
