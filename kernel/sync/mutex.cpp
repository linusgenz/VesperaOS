//
// Created by Linus on 20.07.25.
//

#include <klib/intrusive_queue.h>
#include <units/unit.h>
#include <vespera/scheduling.h>
#include <vespera/sync/atomic.h>
#include <vespera/sync/mutex.h>

namespace kernel {
    inline bool scheduling_started = false;

    void Mutex::init() {
        locked_.init(false);
    }

    void Mutex::lock() {
        if (!scheduling::is_curent_cpu_enabled()) {
            while (locked_.test_and_set()) {
                __asm__ volatile("pause");
            }
            return;
        }

        while (locked_.test_and_set()) {
            Unit *current = scheduling::get_current_unit();
            scheduling::remove_unit(current);

            current->state = UnitState::Blocked;

            {
                SpinlockGuard guard(lock_);
                waiters_.push(current);
            }

            scheduling::yield();
        }
    }

    void Mutex::unlock() {
        Unit *to_wake = nullptr;
        {
            SpinlockGuard guard(lock_);
            to_wake = waiters_.pop();
        }

        locked_.clear();

        if (to_wake) {
            to_wake->state = UnitState::Ready;
            scheduling::add_unit(to_wake);
        }
    }

    bool Mutex::try_lock() {
        return !locked_.test_and_set();
    }

    bool Mutex::is_locked() const {
        return locked_.load();
    }
}  // namespace kernel
