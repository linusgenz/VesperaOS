// pthread.c
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

#include "pthread.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <realm.h>
#include <unit.h>

#include "futex.h"
#include "stdbool.h"

/* -----------------------------------------------------------------------
 * Thread control block
 * ----------------------------------------------------------------------- */

struct __pthread_cb {
    UnitID unit;
    void* (*start_routine)(void*);
    void* arg;
    void* retval;
    int detached;        /* accessed under g_detach_lock */
    int finished;        /* set by the trampoline right before the unit exits */
    uint32_t join_futex; /* 0 while running, bumped to 1 + futex_wake on finish */
};

static pthread_mutex_t g_detach_lock = PTHREAD_MUTEX_INITIALIZER;

static _Thread_local struct __pthread_cb* tls_self = NULL;

static int mutex_lock_slow(pthread_mutex_t* mutex);
static void run_tsd_destructors(void);

/* -----------------------------------------------------------------------
 * Threads
 * ----------------------------------------------------------------------- */

int pthread_attr_init(pthread_attr_t* attr) {
    if (!attr) return EINVAL;
    attr->stack_size = 0;
    attr->detached = PTHREAD_CREATE_JOINABLE;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t* attr) {
    (void)attr;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize) {
    if (!attr) return EINVAL;
    attr->stack_size = stacksize;
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stacksize) {
    if (!attr || !stacksize) return EINVAL;
    *stacksize = attr->stack_size;
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate) {
    if (!attr) return EINVAL;
    attr->detached = detachstate;
    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detachstate) {
    if (!attr || !detachstate) return EINVAL;
    *detachstate = attr->detached;
    return 0;
}

#define PTHREAD_DEFAULT_STACK_SIZE (2u * 1024u * 1024u)

/* Entry point actually passed to spawn_unit. Runs on the new unit, calls the
 * user's start_routine, stashes the result, and wakes any joiner before the
 * unit itself exits. */
static void pthread_trampoline(uint64_t arg_ptr) {
    struct __pthread_cb* cb = (struct __pthread_cb*)(uintptr_t)arg_ptr;

    cb->unit = get_unit_id();
    tls_self = cb;

    void* result = cb->start_routine(cb->arg);
    cb->retval = result;

    run_tsd_destructors();

    pthread_mutex_lock(&g_detach_lock);
    cb->finished = 1;
    const int detached = cb->detached;
    pthread_mutex_unlock(&g_detach_lock);

    /* Publish completion to any pthread_join waiter. */
    __atomic_store_n(&cb->join_futex, 1, __ATOMIC_RELEASE);
    futex_wake_all(&cb->join_futex);

    if (detached) {
        free(cb);
    }

    sys_exit(0, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

int pthread_create(
    pthread_t* thread, const pthread_attr_t* attr,
    void* (*start_routine)(void*), void* arg
) {
    if (!thread || !start_routine) return EINVAL;

    struct __pthread_cb* cb = malloc(sizeof(struct __pthread_cb));
    if (!cb) return ENOMEM;

    cb->start_routine = start_routine;
    cb->arg = arg;
    cb->retval = NULL;
    cb->detached = attr ? attr->detached : PTHREAD_CREATE_JOINABLE;
    cb->finished = 0;
    cb->join_futex = 0;

    const size_t stack_size = (attr && attr->stack_size) ? attr->stack_size : (size_t)PTHREAD_DEFAULT_STACK_SIZE;

    const RealmID realm = get_realm_id();
    const UnitID unit = spawn_unit(realm, (uint64_t)(uintptr_t)pthread_trampoline,
                                   (uint64_t)(uintptr_t)cb, stack_size);

    if ((int64_t)unit < 0) {
        const int err = (int)-(int64_t)unit;
        free(cb);
        return err ? err : EAGAIN;
    }

    *thread = cb;
    return 0;
}


int pthread_join(pthread_t thread, void** retval) {
    if (!thread) return EINVAL;
    struct __pthread_cb* cb = thread;

    pthread_mutex_lock(&g_detach_lock);
    if (cb->detached) {
        pthread_mutex_unlock(&g_detach_lock);
        return EINVAL; /* joining a detached thread is undefined by POSIX; we reject it */
    }
    pthread_mutex_unlock(&g_detach_lock);

    while (__atomic_load_n(&cb->join_futex, __ATOMIC_ACQUIRE) == 0) {
        futex_wait(&cb->join_futex, 0, NULL);
    }

    int64_t exit_code = 0;
    join_unit(cb->unit, &exit_code);


    if (retval) *retval = cb->retval;
    free(cb);
    return 0;
}

int pthread_detach(pthread_t thread) {
    if (!thread) return EINVAL;
    struct __pthread_cb* cb = thread;

    pthread_mutex_lock(&g_detach_lock);
    if (cb->detached) {
        pthread_mutex_unlock(&g_detach_lock);
        return EINVAL;
    }
    cb->detached = 1;
    const int already_finished = cb->finished;
    pthread_mutex_unlock(&g_detach_lock);

    if (already_finished) free(cb);

    return 0;
}

[[noreturn]] void pthread_exit(void* retval) {
    run_tsd_destructors();

    struct __pthread_cb* cb = tls_self;

    if (cb) {
        cb->retval = retval;

        pthread_mutex_lock(&g_detach_lock);
        cb->finished = 1;
        const int detached = cb->detached;
        pthread_mutex_unlock(&g_detach_lock);

        __atomic_store_n(&cb->join_futex, 1, __ATOMIC_RELEASE);
        futex_wake_all(&cb->join_futex);

        if (detached) free(cb);
    }

    sys_exit(0, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

pthread_t pthread_self(void) {
    if (tls_self) return tls_self;

    struct __pthread_cb* cb = malloc(sizeof(struct __pthread_cb));
    if (!cb) return NULL; /* not POSIX-compliant (pthread_self can't fail),
                            * but there's nothing sane to return on OOM here. */

    cb->unit = get_unit_id();
    cb->start_routine = NULL;
    cb->arg = NULL;
    cb->retval = NULL;
    cb->detached = 1; /* not joinable — see comment above */
    cb->finished = 0;
    cb->join_futex = 0;

    tls_self = cb;
    return cb;
}

int pthread_equal(pthread_t a, pthread_t b) {
    return a == b;
}

void pthread_yield_np(void) {
    sched_yield();
}

/* -----------------------------------------------------------------------
 * Mutexes
 * ----------------------------------------------------------------------- */

int pthread_mutexattr_init(pthread_mutexattr_t* attr) {
    if (!attr) return EINVAL;
    attr->type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t* attr) {
    (void)attr;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type) {
    if (!attr) return EINVAL;
    attr->type = type;
    return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type) {
    if (!attr || !type) return EINVAL;
    *type = attr->type;
    return 0;
}

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    if (!mutex) return EINVAL;
    mutex->state = 0;
    mutex->type = attr ? attr->type : PTHREAD_MUTEX_DEFAULT;
    mutex->owner = 0;
    mutex->rec_count = 0;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;
    return 0;
}

static int mutex_try_fastpath(pthread_mutex_t* mutex) {
    uint32_t expected = 0;
    return __atomic_compare_exchange_n(&mutex->state, &expected, 1,
                                       false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

static int mutex_lock_slow(pthread_mutex_t* mutex) {
    uint32_t state = __atomic_exchange_n(&mutex->state, 2, __ATOMIC_ACQUIRE);
    while (state != 0) {
        futex_wait(&mutex->state, 2, NULL);
        state = __atomic_exchange_n(&mutex->state, 2, __ATOMIC_ACQUIRE);
    }
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        const UnitID self = get_unit_id();
        if (mutex->owner == self) {
            mutex->rec_count++;
            return 0;
        }
    }

    if (!mutex_try_fastpath(mutex)) mutex_lock_slow(mutex);

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE || mutex->type == PTHREAD_MUTEX_ERRORCHECK) {
        mutex->owner = get_unit_id();
        mutex->rec_count = 1;
    }
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        const UnitID self = get_unit_id();
        if (mutex->owner == self) {
            mutex->rec_count++;
            return 0;
        }
    }

    if (!mutex_try_fastpath(mutex)) return EBUSY;

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE || mutex->type == PTHREAD_MUTEX_ERRORCHECK) {
        mutex->owner = get_unit_id();
        mutex->rec_count = 1;
    }
    return 0;
}

int pthread_mutex_timedlock(pthread_mutex_t* mutex, const struct timespec* abstime) {
    if (!mutex) return EINVAL;

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        const UnitID self = get_unit_id();
        if (mutex->owner == self) {
            mutex->rec_count++;
            return 0;
        }
    }

    if (mutex_try_fastpath(mutex)) {
        if (mutex->type == PTHREAD_MUTEX_RECURSIVE || mutex->type == PTHREAD_MUTEX_ERRORCHECK) {
            mutex->owner = get_unit_id();
            mutex->rec_count = 1;
        }
        return 0;
    }

    uint32_t state = __atomic_exchange_n(&mutex->state, 2, __ATOMIC_ACQUIRE);
    while (state != 0) {
        if (futex_wait_until(&mutex->state, 2, abstime) != 0) {
            /* Timed out (or real error) — report ETIMEDOUT per POSIX. */
            return ETIMEDOUT;
        }
        state = __atomic_exchange_n(&mutex->state, 2, __ATOMIC_ACQUIRE);
    }

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE || mutex->type == PTHREAD_MUTEX_ERRORCHECK) {
        mutex->owner = get_unit_id();
        mutex->rec_count = 1;
    }
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE && mutex->rec_count > 0) {
        mutex->rec_count--;
        if (mutex->rec_count > 0) return 0;
        mutex->owner = 0;
    } else if (mutex->type == PTHREAD_MUTEX_ERRORCHECK) {
        if (mutex->owner != get_unit_id()) return EPERM;
        mutex->owner = 0;
        mutex->rec_count = 0;
    }

    const uint32_t prev = __atomic_exchange_n(&mutex->state, 0, __ATOMIC_RELEASE);
    if (prev == 2) {
        /* There may have been waiters sleeping on the old state — wake one.
         * (Waking exactly one is the standard futex-mutex optimization;
         * the woken thread re-announces state=2 itself if it doesn't win
         * the race, so no wakeups are lost.) */
        futex_wake(&mutex->state, 1);
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * Condition variables
 * ----------------------------------------------------------------------- */

int pthread_condattr_init(pthread_condattr_t* attr) {
    if (!attr) return EINVAL;
    attr->unused = 0;
    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t* attr) {
    (void)attr;
    return 0;
}

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
    (void)attr;
    if (!cond) return EINVAL;
    cond->seq = 0;
    return 0;
}

int pthread_cond_destroy(pthread_cond_t* cond) {
    (void)cond;
    return 0;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    if (!cond || !mutex) return EINVAL;

    const uint32_t seq = __atomic_load_n(&cond->seq, __ATOMIC_ACQUIRE);
    pthread_mutex_unlock(mutex);
    futex_wait(&cond->seq, seq, NULL);
    pthread_mutex_lock(mutex);
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex, const struct timespec* abstime) {
    if (!cond || !mutex) return EINVAL;

    const uint32_t seq = __atomic_load_n(&cond->seq, __ATOMIC_ACQUIRE);
    pthread_mutex_unlock(mutex);
    const int wait_ret = futex_wait_until(&cond->seq, seq, abstime);
    pthread_mutex_lock(mutex);

    return (wait_ret != 0) ? ETIMEDOUT : 0;
}

int pthread_cond_signal(pthread_cond_t* cond) {
    if (!cond) return EINVAL;
    __atomic_fetch_add(&cond->seq, 1, __ATOMIC_RELEASE);
    futex_wake(&cond->seq, 1);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
    if (!cond) return EINVAL;
    __atomic_fetch_add(&cond->seq, 1, __ATOMIC_RELEASE);
    futex_wake_all(&cond->seq);
    return 0;
}

/* -----------------------------------------------------------------------
 * Barriers
 * ----------------------------------------------------------------------- */

int pthread_barrier_init(pthread_barrier_t* barrier, const pthread_barrierattr_t* attr, unsigned count) {
    (void)attr;
    if (!barrier || count == 0) return EINVAL;
    barrier->count = count;
    barrier->waiting = 0;
    barrier->generation = 0;
    pthread_mutex_init(&barrier->lock, NULL);
    return 0;
}

int pthread_barrier_destroy(pthread_barrier_t* barrier) {
    if (!barrier) return EINVAL;
    pthread_mutex_destroy(&barrier->lock);
    return 0;
}

int pthread_barrier_wait(pthread_barrier_t* barrier) {
    if (!barrier) return EINVAL;

    pthread_mutex_lock(&barrier->lock);
    const uint32_t gen = barrier->generation;
    barrier->waiting++;

    if (barrier->waiting == barrier->count) {
        /* Last thread to arrive: trip the barrier and release everyone. */
        barrier->waiting = 0;
        barrier->generation++;
        pthread_mutex_unlock(&barrier->lock);
        futex_wake_all(&barrier->generation);
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }

    pthread_mutex_unlock(&barrier->lock);
    while (__atomic_load_n(&barrier->generation, __ATOMIC_ACQUIRE) == gen) {
        futex_wait(&barrier->generation, gen, NULL);
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * Once
 * ----------------------------------------------------------------------- */

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine) return EINVAL;

    uint32_t expected = 0;
    if (__atomic_compare_exchange_n(&once_control->state, &expected, 1,
                                    false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        init_routine();
        __atomic_store_n(&once_control->state, 2, __ATOMIC_RELEASE);
        futex_wake_all(&once_control->state);
        return 0;
    }

    /* Someone else is running (or already ran) the initializer — wait for state==2. */
    while (__atomic_load_n(&once_control->state, __ATOMIC_ACQUIRE) != 2) {
        futex_wait(&once_control->state, 1, NULL);
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * Thread-specific storage
 * ----------------------------------------------------------------------- */

#define PTHREAD_KEYS_MAX 128
#define PTHREAD_DESTRUCTOR_ITERATIONS 4

static void (*g_key_destructors[PTHREAD_KEYS_MAX])(void*);
static int g_key_used[PTHREAD_KEYS_MAX];
static uint64_t g_key_generation[PTHREAD_KEYS_MAX];
static pthread_mutex_t g_key_lock = PTHREAD_MUTEX_INITIALIZER;

/* Per-thread values and the generation each was written under — both
 * _Thread_local, so every unit has its own independent copy. */
static _Thread_local void* tls_key_values[PTHREAD_KEYS_MAX];
static _Thread_local uint32_t tls_key_gen[PTHREAD_KEYS_MAX];

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*)) {
    if (!key) return EINVAL;

    pthread_mutex_lock(&g_key_lock);
    for (unsigned int i = 0; i < PTHREAD_KEYS_MAX; i++) {
        if (!g_key_used[i]) {
            g_key_used[i] = 1;
            g_key_destructors[i] = destructor;

            const uint32_t new_gen = __atomic_fetch_add(&g_key_generation[i], 1, __ATOMIC_RELEASE) + 1;

            key->slot = i;
            key->seq = new_gen;
            pthread_mutex_unlock(&g_key_lock);

            tls_key_values[i] = NULL;
            tls_key_gen[i] = new_gen;
            return 0;
        }
    }
    pthread_mutex_unlock(&g_key_lock);
    return EAGAIN;
}

int pthread_key_delete(pthread_key_t key) {
    if (key.slot >= PTHREAD_KEYS_MAX) return EINVAL;

    pthread_mutex_lock(&g_key_lock);

    if (__atomic_load_n(&g_key_generation[key.slot], __ATOMIC_RELAXED) != key.seq || !g_key_used[key.slot]) {
        pthread_mutex_unlock(&g_key_lock);
        return EINVAL;
    }

    g_key_used[key.slot] = 0;
    g_key_destructors[key.slot] = NULL;

    __atomic_fetch_add(&g_key_generation[key.slot], 1, __ATOMIC_RELEASE);

    pthread_mutex_unlock(&g_key_lock);
    return 0;
}

int pthread_setspecific(pthread_key_t key, const void* value) {
    if (key.slot >= PTHREAD_KEYS_MAX) return EINVAL;

    const uint32_t cur_gen = __atomic_load_n(&g_key_generation[key.slot], __ATOMIC_ACQUIRE);
    if (key.seq != cur_gen) {
        return EINVAL;
    }

    tls_key_values[key.slot] = (void*)value;
    tls_key_gen[key.slot] = cur_gen;
    return 0;
}

void* pthread_getspecific(pthread_key_t key) {
    if (key.slot >= PTHREAD_KEYS_MAX) return NULL;

    const uint32_t cur_gen = __atomic_load_n(&g_key_generation[key.slot], __ATOMIC_ACQUIRE);
    if (key.seq != cur_gen) {
        /* Stale handle, POSIX behavior for an invalid key is undefined */
        return NULL;
    }

    if (tls_key_gen[key.slot] != cur_gen) {
        return NULL;
    }

    return tls_key_values[key.slot];
}

/* Runs every key's destructor for the calling thread's non-NULL values. */
static void run_tsd_destructors(void) {
    for (int pass = 0; pass < PTHREAD_DESTRUCTOR_ITERATIONS; pass++) {
        int any_ran = 0;

        for (unsigned int i = 0; i < PTHREAD_KEYS_MAX; i++) {
            if (tls_key_values[i] == NULL) continue;

            pthread_mutex_lock(&g_key_lock);
            const int in_use = g_key_used[i];
            void (*destr)(void*) = g_key_destructors[i];
            const uint32_t cur_gen = __atomic_load_n(&g_key_generation[i], __ATOMIC_RELAXED);
            pthread_mutex_unlock(&g_key_lock);

            if (!in_use || !destr || tls_key_gen[i] != cur_gen) continue;

            void* val = tls_key_values[i];
            tls_key_values[i] = NULL;
            any_ran = 1;

            destr(val);
        }

        if (!any_ran) break;
    }
}

/* -----------------------------------------------------------------------
 * Read-write locks
 * ----------------------------------------------------------------------- */

int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr) {
    (void)attr;
    if (!rwlock) return EINVAL;
    pthread_mutex_init(&rwlock->lock, NULL);
    pthread_cond_init(&rwlock->readers_ok, NULL);
    pthread_cond_init(&rwlock->writer_ok, NULL);
    rwlock->active_readers = 0;
    rwlock->active_writer = 0;
    rwlock->waiting_writers = 0;
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    pthread_mutex_destroy(&rwlock->lock);
    return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    pthread_mutex_lock(&rwlock->lock);
    while (rwlock->active_writer || rwlock->waiting_writers > 0) {
        pthread_cond_wait(&rwlock->readers_ok, &rwlock->lock);
    }
    rwlock->active_readers++;
    pthread_mutex_unlock(&rwlock->lock);
    return 0;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    pthread_mutex_lock(&rwlock->lock);
    if (rwlock->active_writer || rwlock->waiting_writers > 0) {
        pthread_mutex_unlock(&rwlock->lock);
        return EBUSY;
    }
    rwlock->active_readers++;
    pthread_mutex_unlock(&rwlock->lock);
    return 0;
}

int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    pthread_mutex_lock(&rwlock->lock);
    rwlock->waiting_writers++;
    while (rwlock->active_writer || rwlock->active_readers > 0) {
        pthread_cond_wait(&rwlock->writer_ok, &rwlock->lock);
    }
    rwlock->waiting_writers--;
    rwlock->active_writer = 1;
    pthread_mutex_unlock(&rwlock->lock);
    return 0;
}

int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    pthread_mutex_lock(&rwlock->lock);
    if (rwlock->active_writer || rwlock->active_readers > 0) {
        pthread_mutex_unlock(&rwlock->lock);
        return EBUSY;
    }
    rwlock->active_writer = 1;
    pthread_mutex_unlock(&rwlock->lock);
    return 0;
}

int pthread_rwlock_unlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    pthread_mutex_lock(&rwlock->lock);
    if (rwlock->active_writer) {
        rwlock->active_writer = 0;
    } else if (rwlock->active_readers > 0) {
        rwlock->active_readers--;
    }

    if (rwlock->active_readers == 0) {
        /* Prefer waking a writer once no readers remain, to bound writer
         * starvation somewhat (still not truly fair, see file header). */
        pthread_cond_signal(&rwlock->writer_ok);
    }
    if (rwlock->waiting_writers == 0) {
        pthread_cond_broadcast(&rwlock->readers_ok);
    }
    pthread_mutex_unlock(&rwlock->lock);
    return 0;
}
