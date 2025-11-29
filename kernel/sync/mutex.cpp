//
// Created by Linus on 20.07.25.
//

#include "../../include/kernel/sync/mutex.h"

#include <kernel/scheduling.h>

#include <kernel/sync/atomic.h>

namespace kernel {
    inline bool scheduling_started = false;


    void mutex_t::init() {
        locked.init(false);
    }

    void mutex_t::lock() {
        if (!scheduling::is_curent_cpu_enabled()) {
            while (locked.test_and_set()) {
                __asm__ volatile("pause");
            }
            return;
        }

        // Versuche Lock zu erwerben
        while (locked.test_and_set()) {
            Unit *current = scheduling::get_current_unit();
            scheduling::remove_unit(current);

            current->state = UNIT_BLOCKED;

            waiters.push(current);

            scheduling::yield();
        }
    }

    void mutex_t::unlock() {
        Unit *to_wake = waiters.pop();

        locked.clear();

        if (to_wake) {
            to_wake->state = UNIT_READY;
            scheduling::add_unit(to_wake);
        }
    }

    bool mutex_t::try_lock() {
        return !locked.test_and_set();
    }


    bool mutex_t::is_locked() const {
        return locked.load();
    }
}
