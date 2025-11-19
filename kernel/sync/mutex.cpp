//
// Created by Linus on 20.07.25.
//

#include "mutex.h"

#include "../../include/log.h"
#include <scheduling.h>

#include "../cpu/cpu_manager.h"

namespace kernel {

    void mutex_init(mutex_t* m) {
        m->locked = false;
        m->waiters = nullptr;
    }

    void mutex_lock(mutex_t* m) {
        if (!scheduling::cpu_scheduler::get_cpu_data(CPUManager::get_current_cpu_id())->scheduler_enabled) {
            // Nur spinlock, nicht blockieren!
            while (__sync_lock_test_and_set(&m->locked, true)) {
                // Busy wait, kein Threadwechsel!
            }
            return;
        }
        while (__sync_lock_test_and_set(&m->locked, true)) {
            Unit* current = scheduling::get_current_unit();
            current->state = UNIT_BLOCKED;

            // In Warteliste einfügen
            current->next = m->waiters;
            m->waiters = current;

            scheduling::yield(); // Blockiere diesen Thread
        }
    }

    void mutex_unlock(mutex_t* m) {
        m->locked = false;

        if (m->waiters) {
            Unit* to_wake = m->waiters;
            m->waiters = m->waiters->next;

            to_wake->state = UNIT_READY;
            scheduling::add_unit(to_wake);
        }
    }

}
