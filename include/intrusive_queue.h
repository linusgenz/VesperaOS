// intrusive_queue.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 21.11.25.
//
// This file is part of VesperaOS.
// 
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUIntrusiveNode ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#ifndef VESPERAOS_INTRUSIVE_QUEUE_H
#define VESPERAOS_INTRUSIVE_QUEUE_H

#include <concepts>
#include <log.h>

#include "../kernel/sync/spinlock.h"
#include "../kernel/utils/panic.h"

template<typename T>
concept IntrusiveNode = requires(T t)
{
    { t.next } -> std::same_as<T *>;
};

struct queue_lock_normal {
    using guard_t = spinlock_guard;
};

struct queue_lock_irq {
    using guard_t = spinlock_guard_irq;
};

template<typename IntrusiveNode, typename LockPolicy = queue_lock_normal>
struct intrusive_queue_t {
    using guard_t = typename LockPolicy::guard_t;

private:

    spinlock_t lock{};

public:
    intrusive_queue_t() {
        lock.init();
    }
    IntrusiveNode *head = nullptr;
    IntrusiveNode *tail = nullptr;
    intrusive_queue_t(const intrusive_queue_t &) = delete;
    intrusive_queue_t &operator=(const intrusive_queue_t &) = delete;

    // FIFO
    void push(IntrusiveNode* element) {
        guard_t g(lock);

        element->next = nullptr;
        if (!head) {
            head = tail = element;
        } else {
            tail->next = element;
            tail = element;
        }
    }

    // LIFO
    void push_front(IntrusiveNode* element) {
        guard_t g(lock);

        element->next = head;
        head = element;
        if (!tail) tail = element;
    }

    // FIFO
    IntrusiveNode* pop() {
        guard_t g(lock);
        if (!head) return nullptr;

        IntrusiveNode* element = head;
        head = head->next;
        if (!head) tail = nullptr;
        element->next = nullptr;
        return element;
    }

    // Remove specific element
    bool remove(IntrusiveNode* target) {
        guard_t g(lock);
        if (!head) return false;

        if (head == target) {
            head = head->next;
            if (!head) tail = nullptr;
            target->next = nullptr;
            return true;
        }

        IntrusiveNode* prev = head;
        IntrusiveNode* cur = head->next;
        while (cur) {
            if (cur == target) {
                prev->next = cur->next;
                if (cur == tail) tail = prev;
                cur->next = nullptr;
                return true;
            }
            prev = cur;
            cur = cur->next;
        }
        return false;
    }

    template<typename Predicate>
    IntrusiveNode* extract_if(Predicate&& pred, IntrusiveNode** out_tail = nullptr) {
        guard_t g(lock);

        IntrusiveNode* result_head = nullptr;
        IntrusiveNode* result_tail = nullptr;

        IntrusiveNode* prev = nullptr;
        IntrusiveNode* cur = head;

        while (cur) {
            IntrusiveNode* next = cur->next;
            if (pred(cur)) {
                // Entferne aus Queue
                if (prev) prev->next = next;
                else head = next;

                if (cur == tail) tail = prev;

                cur->next = nullptr;
                if (result_tail) {
                    result_tail->next = cur;
                    result_tail = cur;
                } else {
                    result_head = result_tail = cur;
                }
            } else {
                prev = cur;
            }
            cur = next;
        }

        if (out_tail) *out_tail = result_tail;
        return result_head;
    }

    void append_list(IntrusiveNode *list_head, IntrusiveNode *list_tail) {
        if (!list_head) return;

        guard_t g(lock);

        IntrusiveNode* tmp = list_head;
        while (tmp) {
            tmp = tmp->next;
        }

        // ensure detached
        SET_NEXT(list_tail, nullptr);

        if (!head) {
            head = list_head;
            tail = list_tail;
        } else {
            SET_NEXT(tail, list_head);
            tail = list_tail;
        }

    }

    [[nodiscard]] bool empty() const {
        return head == nullptr;
    }

    [[nodiscard]] IntrusiveNode *front() {
        guard_t g(lock);
        return head;
    }

    void clear() {
        guard_t g(lock);
        head = nullptr;
        tail = nullptr;
    }
};

#endif //VESPERAOS_INTRUSIVE_QUEUE_H
