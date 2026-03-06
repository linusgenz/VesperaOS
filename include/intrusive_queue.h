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
#include <kernel/sync/spinlock.h>

template<typename T>
concept IntrusiveNode = requires(T t)
{
    { t.next } -> std::same_as<T *>;
};

struct QueueLockNormal {
    using guard_t = SpinlockGuard;
};

struct QueueLockIrq {
    using guard_t = SpinlockGuardIrq;
};

template<typename Node, typename LockPolicy = QueueLockNormal>
class IntrusiveQueue {
    using guard_t = LockPolicy::guard_t;

    Spinlock lock_{};
    Node *head_ = nullptr;
    Node *tail_ = nullptr;

public:
    IntrusiveQueue() : head_(nullptr), tail_(nullptr) {
        lock_.init();
    }


    IntrusiveQueue(const IntrusiveQueue &) = delete;

    IntrusiveQueue &operator=(const IntrusiveQueue &) = delete;

    // FIFO
    void push(Node *element) {
        guard_t g(lock_);

        if (!head_) {
            head_ = tail_ = element;
            element->next = nullptr;
        } else {
            tail_->next = element;
            tail_ = element;
            element->next = nullptr;
        }
    }

    // LIFO
    void push_front(Node *element) {
        guard_t g(lock_);
        if (element->cpu_id == 5) {
        //    Log::debug("ADDED: %u %u cpu: %u", element->is_idle, element->id, element->cpu_id);
        }
        element->next = head_;
        head_ = element;
        if (!tail_) tail_ = element;
    }

    // FIFO
    Node *pop() {
        guard_t g(lock_);
        if (!head_) return nullptr;

        Node *element = head_;
        head_ = head_->next;
        if (!head_) tail_ = nullptr;
        element->next = nullptr;
        return element;
    }

    // Remove specific element
    bool remove(Node *target) {
        guard_t g(lock_);
        if (!head_) return false;

        if (head_ == target) {
            head_ = head_->next;
            if (!head_) tail_ = nullptr;
            target->next = nullptr;
            return true;
        }

        Node *prev = head_;
        Node *cur = head_->next;
        while (cur) {
            if (cur == target) {
                prev->next = cur->next;
                if (cur == tail_) tail_ = prev;
                cur->next = nullptr;
                return true;
            }
            prev = cur;
            cur = cur->next;
        }
        return false;
    }

    template<typename Predicate>
    Node *extract_if(Predicate &&pred, Node **out_tail = nullptr) {
        guard_t g(lock_);

        Node *result_head = nullptr;
        Node *result_tail = nullptr;

        Node *prev = nullptr;
        Node *cur = head_;

        while (cur) {
            Node *next = cur->next;
            if (pred(cur)) {
                // Entferne aus Queue
                if (prev) prev->next = next;
                else head_ = next;

                if (cur == tail_) tail_ = prev;

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

    [[nodiscard]] bool empty() const {
        return head_ == nullptr;
    }

    [[nodiscard]] Node *front() {
        guard_t g(lock_);
        return head_;
    }

    void clear() {
        guard_t g(lock_);
        head_ = nullptr;
        tail_ = nullptr;
    }
};

#endif //VESPERAOS_INTRUSIVE_QUEUE_H
