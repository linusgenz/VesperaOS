// thread.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 08.09.25.
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

#ifndef VESPERAOS_THREAD_H
#define VESPERAOS_THREAD_H

#include "cstdint"
#include "../proc/process.h"

#define THREAD_STACK_SIZE 0x2000

// TODO refactor syscall relevant info see: syscall.asm into own struct inside kthread
enum ThreadState {
    THREAD_NEW = 0,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_WAITING,
    THREAD_TERMINATED
};

struct kthread_t {
    uint64_t tid;
    char name[64];

    ThreadState state;
    uint64_t wakeup_tick;
    void *stack;
    void *stack_top;
    void *stack_pointer;
    uint64_t stack_size;
    uint8_t priority;
    void (*entry)(void *);

    void *arg;
    uint8_t cpu_id;
    bool is_user_thread;
    void *saved_user_rsp;
    void *user_stack_top;
    void *user_entry;
    bool is_idle_thread;
    bool from_syscall;
    void *kernel_rsp_after_sleep;
    void *kernel_rsp_after_syscall;
    int64_t exit_code;

    uint64_t creation_time;

    kthread_t *prev;
    kthread_t *next;
    kthread_t *next_in_process;
    kprocess_t *process;
};

struct sleeping_thread_t {
    kthread_t *thread;
    uint64_t wakeup_tick;
    sleeping_thread_t *next;
};

#endif //VESPERAOS_THREAD_H