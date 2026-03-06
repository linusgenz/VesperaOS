//
// Created by Linus on 17.07.25.
//

#ifndef SPINLOCK_H
#define SPINLOCK_H
#include <stdint.h>

class Spinlock {
    volatile uint32_t locked_{0};

   public:
    void init(const char *name = "unnamed_lock");

    void lock();

    void unlock();

    void lock_irqsave(uint64_t &flags) {
        flags = irq_save();
        lock();
    }

    void unlock_irqrestore(const uint64_t flags) {
        unlock();
        irq_restore(flags);
    }

   private:
    uint32_t xchg(volatile uint32_t *ptr, uint32_t val) {
        uint32_t old = 0;
        __asm__ volatile("lock xchg %0, %1" : "=r"(old), "+m"(*ptr) : "0"(val) : "memory");
        return old;
    }

    static uint64_t irq_save() {
        uint64_t flags = 0;
        asm volatile(
            "pushfq\n\t"
            "popq %0\n\t"
            "cli"
            : "=r"(flags)
            :
            : "memory"
        );
        return flags;
    }

    static void irq_restore(uint64_t flags) {
        asm volatile(
            "pushq %0\n\t"
            "popfq"
            :
            : "r"(flags)
            : "memory", "cc"
        );
    }
};

struct SpinlockGuard {
    Spinlock &lock_ref;

    explicit SpinlockGuard(Spinlock &lock)
        : lock_ref(lock) {
        lock_ref.lock();
    }

    ~SpinlockGuard() {
        lock_ref.unlock();
    }

    SpinlockGuard(const SpinlockGuard &) = delete;

    SpinlockGuard &operator=(const SpinlockGuard &) = delete;
};

struct SpinlockGuardIrq {
    Spinlock &lock_ref;
    uint64_t flags{};

    explicit SpinlockGuardIrq(Spinlock &lock)
        : lock_ref(lock) {
        lock_ref.lock_irqsave(flags);
    }

    ~SpinlockGuardIrq() {
        lock_ref.unlock_irqrestore(flags);
    }

    SpinlockGuardIrq(const SpinlockGuardIrq &) = delete;
    SpinlockGuardIrq &operator=(const SpinlockGuardIrq &) = delete;
};

#endif  // SPINLOCK_H
