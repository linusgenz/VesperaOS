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
    class Mutex {
        atomic_flag_t locked_{};
        IntrusiveQueue<Unit, QueueLockIrq> waiters_{};

       public:
        void init();

        void lock();

        void unlock();

        bool try_lock();

        [[nodiscard]] bool is_locked() const;
    };

    struct MutexGuard {
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
