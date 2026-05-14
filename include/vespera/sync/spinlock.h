//
// Created by Linus on 17.07.25.
//

#ifndef SPINLOCK_H
#define SPINLOCK_H
#include <vespera/types.h>

class Spinlock {
    volatile u32 locked_{0};

   public:
    void init(const char *name = "unnamed_lock");

    void lock();

    void unlock();

    void lock_irqsave(u64 &flags) {
        flags = irq_save();
        lock();
    }

    void unlock_irqrestore(const u64 flags) {
        unlock();
        irq_restore(flags);
    }

   private:
    u32 xchg(volatile u32 *ptr, u32 val) {
        u32 old = 0;
        __asm__ volatile("lock xchg %0, %1" : "=r"(old), "+m"(*ptr) : "0"(val) : "memory");
        return old;
    }

    static u64 irq_save() {
        u64 flags = 0;
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

    static void irq_restore(u64 flags) {
        asm volatile(
            "pushq %0\n\t"
            "popfq"
            :
            : "r"(flags)
            : "memory", "cc"
        );
    }
};

struct [[jetbrains::guard]] SpinlockGuard {
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

struct [[jetbrains::guard]] SpinlockGuardIrq {
    Spinlock &lock_ref;
    u64 flags{};

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
