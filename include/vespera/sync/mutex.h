//
// Created by Linus on 20.07.25.
//

#ifndef MUTEX_H
#define MUTEX_H
#include "atomic.h"
#include <klib/intrusive_queue.h>
#include <vespera/sync/spinlock.h>

class Unit;

namespace kernel {
    /**
     * @brief Mutex for thread synchronization
     *
     * Behaves differently depending on context:
     * - Before scheduling start: Simple spinlock
     * - After scheduling start: Blocking mutex with waiting list
     */
    class Mutex {
        atomic_flag_t locked_{};
        Spinlock lock_;
        IntrusiveQueue<Unit> waiters_;

       public:
        void init();

        void lock();

        void unlock();

        bool try_lock();

        [[nodiscard]] bool is_locked() const;
    };

    struct [[jetbrains::guard]] MutexGuard {
        Mutex &mtx;

        explicit MutexGuard(Mutex &m)
            : mtx(m) {
            mtx.lock();
        }

        ~MutexGuard() {
            mtx.unlock();
        }

        MutexGuard(const MutexGuard &) = delete;

        MutexGuard &operator=(const MutexGuard &) = delete;
    };
}  // namespace kernel

#endif  // MUTEX_H
