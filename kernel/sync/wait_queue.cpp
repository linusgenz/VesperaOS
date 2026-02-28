// wait_queue.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 23.11.25.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include "../../include/kernel/sync/wait_queue.h"

#include <kernel/scheduling.h>

#include "../units/unit.h"

void wait_queue_t::add_wait(Unit *u) {
    auto *entry = new wait_queue_entry_t();
    entry->unit = u;
    entry->next = nullptr;

    {
        spinlock_guard guard(lock);

        if (!head) {
            head = tail = entry;
        } else {
            tail->next = entry;
            tail = entry;
        }
    }

    u->state = UNIT_BLOCKED;

    kernel::scheduling::remove_unit(u);
}

void wait_queue_t::wake_all() {
    spinlock_guard guard(lock);

    wait_queue_entry_t *entry = head;
    while (entry) {
        if (entry->unit) {
            entry->unit->state = UNIT_READY;

            kernel::scheduling::add_unit(entry->unit);
        }

        wait_queue_entry_t *next = entry->next;
        delete entry;
        entry = next;
    }

    head = tail = nullptr;
}

void wait_queue_t::wake_one() {
    spinlock_guard guard(lock);

    if (!head) return;

    wait_queue_entry_t *entry = head;
    head = head->next;
    if (!head) tail = nullptr;

    if (entry->unit) {
        entry->unit->state = UNIT_READY;
        kernel::scheduling::add_unit(entry->unit);
    }

    delete entry;
}

bool wait_queue_t::remove(const Unit *u) {
    spinlock_guard guard(lock);

    if (!head) return false;

    if (head->unit == u) {
        wait_queue_entry_t *tmp = head;
        head = head->next;
        if (!head) tail = nullptr;
        delete tmp;
        return true;
    }

    wait_queue_entry_t *prev = head;
    wait_queue_entry_t *cur = head->next;

    while (cur) {
        if (cur->unit == u) {
            prev->next = cur->next;
            if (cur == tail) tail = prev;
            delete cur;
            return true;
        }
        prev = cur;
        cur = cur->next;
    }

    return false;
}