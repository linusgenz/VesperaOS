//
// Created by Linus on 20.07.25.
//

#ifndef MUTEX_H
#define MUTEX_H
#include <intrusive_queue.h>

#include "atomic.h"

class Unit;

namespace kernel {
    /**
     * @brief Mutex for thread synchronization
     *
     * Behaves differently depending on context:
     * - Before scheduling start: Simple spinlock
     * - After scheduling start: Blocking mutex with waiting list
     */
    struct mutex_t {
        atomic_flag_t locked{};
        intrusive_queue_t<Unit, queue_lock_irq> waiters{};

        void init();

        void lock();

        void unlock();

        bool try_lock();

        [[nodiscard]] bool is_locked() const;
    };

    struct mutex_guard {
        mutex_t &mtx;

        explicit mutex_guard(mutex_t &m) : mtx(m) {
            mtx.lock();
        }

        ~mutex_guard() {
            mtx.unlock();
        }

        mutex_guard(const mutex_guard &) = delete;

        mutex_guard &operator=(const mutex_guard &) = delete;
    };
} // namespace kernel

#endif //MUTEX_H
