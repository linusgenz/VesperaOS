//
// Created by Linus on 17.07.25.
//

#ifndef SPINLOCK_H
#define SPINLOCK_H
#include <cstdint>


struct spinlock_t {
    spinlock_t() : locked(0) {}

    volatile uint32_t locked;

    void init(const char* name = "unnamed_lock");

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
        uint32_t old;
        __asm__ volatile (
            "lock xchg %0, %1"
            : "=r"(old), "+m"(*ptr)
            : "0"(val)
            : "memory"
        );
        return old;
    }

    static uint64_t irq_save() {
        uint64_t flags;
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

struct spinlock_guard {
    spinlock_t &lock_ref;

    explicit spinlock_guard(spinlock_t &lock) : lock_ref(lock) {
        lock_ref.lock();
    }

    ~spinlock_guard() {
        lock_ref.unlock();
    }

    spinlock_guard(const spinlock_guard &) = delete;

    spinlock_guard &operator=(const spinlock_guard &) = delete;
};

struct spinlock_guard_irq {
    spinlock_t &lock_ref;
    uint64_t flags{};

    explicit spinlock_guard_irq(spinlock_t &lock) : lock_ref(lock) {
        lock_ref.lock_irqsave(flags);
    }

    ~spinlock_guard_irq() {
        lock_ref.unlock_irqrestore(flags);
    }

    spinlock_guard_irq(const spinlock_guard_irq &) = delete;
    spinlock_guard_irq &operator=(const spinlock_guard_irq &) = delete;
};


#endif //SPINLOCK_H
