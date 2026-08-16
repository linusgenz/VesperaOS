// pthread.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.08.26.
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

#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#include <unit.h>
#include <futex.h>

#include "signal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Types
 * ----------------------------------------------------------------------- */
typedef struct __pthread_cb* pthread_t;

typedef struct pthread_attr_t {
    size_t stack_size;   /* 0 = use implementation default */
    int    detached;     /* PTHREAD_CREATE_JOINABLE / _DETACHED */
} pthread_attr_t;

/* Futex-backed mutex.
 *   0 = unlocked
 *   1 = locked, no waiters
 *   2 = locked, waiters may be sleeping on futex_wait
 * This lets the fast (uncontended) path do a single atomic CAS with no
 * syscall, and only pay for futex_wake when a waiter might actually exist. */
typedef struct pthread_mutex_t {
    uint32_t state;
    int      type;       /* PTHREAD_MUTEX_* */
    UnitID   owner;       /* 0 = unowned; used only for PTHREAD_MUTEX_ERRORCHECK/RECURSIVE */
    uint32_t rec_count;   /* recursion depth for PTHREAD_MUTEX_RECURSIVE */
} pthread_mutex_t;

typedef struct pthread_mutexattr_t {
    int type;
} pthread_mutexattr_t;

typedef struct pthread_cond_t {
    uint32_t seq;
    clockid_t clock;   /* clock used for pthread_cond_timedwait's abstime; set at
                         * init time from the condattr (or CLOCK_REALTIME by default,
                         * per POSIX). Only CLOCK_REALTIME and CLOCK_MONOTONIC are
                         * meaningful here. */
} pthread_cond_t;

typedef struct pthread_condattr_t {
    clockid_t clock;
} pthread_condattr_t;

typedef struct pthread_barrier_t {
    uint32_t count;       /* threads required to trip the barrier */
    uint32_t waiting;     /* threads currently blocked */
    uint32_t generation;  /* bumped each time the barrier trips, for futex_wait correctness */
    pthread_mutex_t lock;
} pthread_barrier_t;

typedef struct pthread_barrierattr_t {
    int unused;
} pthread_barrierattr_t;

typedef struct pthread_once_t {
    uint32_t state; /* 0 = not run, 1 = running, 2 = done */
} pthread_once_t;

typedef struct pthread_key_t {
    unsigned int slot;
    uint32_t seq;
} pthread_key_t;

typedef struct pthread_rwlock_t {
    pthread_mutex_t lock;      /* guards the fields below */
    pthread_cond_t  readers_ok;
    pthread_cond_t  writer_ok;
    int             active_readers;
    int             active_writer;
    int             waiting_writers;
} pthread_rwlock_t;

typedef struct pthread_rwlockattr_t {
    int unused;
} pthread_rwlockattr_t;

/* -----------------------------------------------------------------------
 * Constants
 * ----------------------------------------------------------------------- */

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#define PTHREAD_MUTEX_NORMAL     0
#define PTHREAD_MUTEX_ERRORCHECK 1
#define PTHREAD_MUTEX_RECURSIVE  2
#define PTHREAD_MUTEX_DEFAULT    PTHREAD_MUTEX_NORMAL

#define PTHREAD_MUTEX_INITIALIZER      { .state = 0, .type = PTHREAD_MUTEX_NORMAL, .owner = 0, .rec_count = 0 }
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER { .state = 0, .type = PTHREAD_MUTEX_RECURSIVE, .owner = 0, .rec_count = 0 }
/* CLOCK_REALTIME is the POSIX default for a condvar created without an
 * explicit condattr (or with a condattr that never called setclock). */
#define PTHREAD_COND_INITIALIZER       { .seq = 0, .clock = CLOCK_REALTIME }
#define PTHREAD_ONCE_INIT              { .state = 0 }

#define PTHREAD_BARRIER_SERIAL_THREAD (-1)

/* -----------------------------------------------------------------------
 * Threads
 * ----------------------------------------------------------------------- */

int pthread_attr_init(pthread_attr_t* attr);
int pthread_attr_destroy(pthread_attr_t* attr);
int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize);
int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stacksize);
int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate);
int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detachstate);

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                    void* (*start_routine)(void*), void* arg);
int pthread_join(pthread_t thread, void** retval);
int pthread_detach(pthread_t thread);
[[noreturn]] void pthread_exit(void* retval);
pthread_t pthread_self(void);
int pthread_equal(pthread_t a, pthread_t b);
void pthread_yield_np(void); /* not POSIX-standard-named but Mesa's c11 shim wants sched_yield semantics */

/* -----------------------------------------------------------------------
 * Mutexes
 * ----------------------------------------------------------------------- */

int pthread_mutexattr_init(pthread_mutexattr_t* attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr);
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);
int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type);

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr);
int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);
int pthread_mutex_timedlock(pthread_mutex_t* mutex, const struct timespec* abstime);
int pthread_mutex_unlock(pthread_mutex_t* mutex);

/* -----------------------------------------------------------------------
 * Condition variables
 * ----------------------------------------------------------------------- */

int pthread_condattr_init(pthread_condattr_t* attr);
int pthread_condattr_destroy(pthread_condattr_t* attr);
int pthread_condattr_setclock(pthread_condattr_t* attr, clockid_t clock_id);
int pthread_condattr_getclock(const pthread_condattr_t* attr, clockid_t* clock_id);

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_destroy(pthread_cond_t* cond);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex, const struct timespec* abstime);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);

/* -----------------------------------------------------------------------
 * Barriers
 * ----------------------------------------------------------------------- */

int pthread_barrier_init(pthread_barrier_t* barrier, const pthread_barrierattr_t* attr, unsigned count);
int pthread_barrier_destroy(pthread_barrier_t* barrier);
int pthread_barrier_wait(pthread_barrier_t* barrier);

/* -----------------------------------------------------------------------
 * Once
 * ----------------------------------------------------------------------- */

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void));

/* -----------------------------------------------------------------------
 * Thread-specific storage
 * ----------------------------------------------------------------------- */

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
int pthread_key_delete(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void* value);
void* pthread_getspecific(pthread_key_t key);

/* -----------------------------------------------------------------------
 * Read-write locks
 * ----------------------------------------------------------------------- */

int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr);
int pthread_rwlock_destroy(pthread_rwlock_t* rwlock);
int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t* rwlock);

int pthread_sigmask(int how, const sigset_t* set, sigset_t* oset);

#ifdef __cplusplus
}
#endif

#endif //_PTHREAD_H