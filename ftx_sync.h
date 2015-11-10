//
// ftx_sync.h
//
// Copyright (c) 2015 Ross MacGregor
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef FTX_SYNC_HPP
#define	FTX_SYNC_HPP

#include <sys/time.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
//
// A C style implementation of mutex and condition variables using futexes.
//
// These synchronization primitives will work across processes using shared
// memory.
//
// The implementation of condition variables has a built in timeout for
// detecting deadlocks. Any operation that takes more than
// FTX_DEADLOCK_TIMEOUT_SEC seconds will be considered deadlocked and an
// EDEADLOCK error will be returned.
//

typedef int32_t ftx_mutex;
typedef struct ftx_condition ftx_condition;

struct ftx_condition
{
    ftx_mutex *m;
    int32_t seq;
    int32_t pad;
};

#define FTX_MUTEX_FREE      0
#define FTX_MUTEX_LOCKED    1
#define FTX_MUTEX_CONTESTED 2

#define FTX_DEADLOCK_TIMEOUT_SEC  10
#define FTX_DEADLOCK_TIMEOUT_NSEC 0

// This function must be called to initialize the mutex before any other mutex
// function is used.
inline void ftx_mutex_init(ftx_mutex *m)
{
    *m = FTX_MUTEX_FREE;
}

// Try to lock the mutex. An EBUSY error is returned if the lock cannot be
// acquired.
inline int ftx_mutex_try_lock(ftx_mutex *m)
{
    // We simply set the mutex to locked if it is not currently locked.
    // It's our lock.
    // Transition FREE -> LOCKED.
    int32_t state = __sync_val_compare_and_swap(m, FTX_MUTEX_FREE, FTX_MUTEX_LOCKED);

    if (state != FTX_MUTEX_FREE)
        return EBUSY;

    return 0;
}

// Try to lock the mutex for set duration. An ETIMEDOUT error is returned if the
// lock cannot be acquired in that time.
inline int ftx_mutex_timed_lock(ftx_mutex *m, const struct timespec *timeout)
{
    // We simply set the mutex to locked if it is not currently locked.
    // It's our lock.
    // Transition FREE -> LOCKED.
    int32_t state = __sync_val_compare_and_swap(m, FTX_MUTEX_FREE, FTX_MUTEX_LOCKED);
    if (state == FTX_MUTEX_FREE)
        return 0;

    // The mutex was previously locked or contested.
    // We will set the mutex to contested and wait for it to become free.

    for(;;)
    {
        // Change lock to contested
        state = __sync_lock_test_and_set(m, FTX_MUTEX_CONTESTED);

        // It's our lock.
        // Transition FREE -> CONTESTED.
        if (state == FTX_MUTEX_FREE)
            return 0;

        // Wait for the mutex to change from the contested state.
        // Transition LOCKED -> CONTESTED.
        int errorCode = syscall(SYS_futex, m, FUTEX_WAIT, FTX_MUTEX_CONTESTED, timeout, NULL, 0);

        assert(errorCode == 0 || (errorCode == -1 && (errno == EAGAIN || errno == ETIMEDOUT)));
        if (errorCode == -1 && errno == ETIMEDOUT)
            return ETIMEDOUT;
    }
}

// Attempt to lock the mutex. An EDEADLOCK error is returned if the mutex is
// locked for more than FTX_DEADLOCK_TIMEOUT_SEC seconds, otherwise the mutex
// is locked on return of this function.
inline int ftx_mutex_lock(ftx_mutex *m)
{
    struct timespec deadlockTimeout = {FTX_DEADLOCK_TIMEOUT_SEC + 2, FTX_DEADLOCK_TIMEOUT_NSEC};

    return ftx_mutex_timed_lock(m, &deadlockTimeout) == ETIMEDOUT ? EDEADLOCK : 0;
}

// Unlock the mutex. The mutex is unlocked on return of this function.
inline void ftx_mutex_unlock(ftx_mutex *m)
{
    int32_t state;

    // Change mutex to to free.
    state = __sync_lock_test_and_set(m, FTX_MUTEX_FREE);

    // Transition LOCKED -> FREE
    if (state == FTX_MUTEX_LOCKED)
        return;

    // Wake a locked thread.
    // Transition CONTESTED -> FREE
    syscall(SYS_futex, m, FUTEX_WAKE, 1, NULL, NULL, 0);
}

// This function must be called to initialize the condition before any other
// condition function is used.
inline void ftx_cond_init(ftx_condition *c)
{
    c->m = NULL;
    c->seq = 0;
}

// This function signals a single waiting thread that the condition has changed.
inline void ftx_cond_signal(ftx_condition *c)
{
    // Increment the sequence number.
    __sync_fetch_and_add(&(c->seq), 1);

    // Wake up a waiting thread.
    syscall(SYS_futex, &(c->seq), FUTEX_WAKE, 1, NULL, NULL, 0);
}

// This function signals all waiting threads that the condition has changed.
inline void ftx_cond_broadcast(ftx_condition *c)
{
    ftx_mutex *m = c->m;

    // No mutex means that there are no waiters
	if (!m) return;

    // Increment the sequence number.
    __sync_fetch_and_add(&(c->seq), 1);

    // Wake one thread, and requeue the rest on the mutex.
    syscall(SYS_futex, &(c->seq), FUTEX_REQUEUE, FTX_MUTEX_LOCKED, (void *) INT_MAX, m, 0);
}

// This function waits for the condition to change for a set duration of time.
// An ETIMEDOUT error is returned if the condition does not change in that time.
// An EDEADLOCK error is returned if the condition mutex is locked for more
// than FTX_DEADLOCK_TIMEOUT_SEC seconds.
inline int ftx_cond_timed_wait(ftx_condition *c, ftx_mutex *m, const struct timespec * timeout)
{
    struct timespec deadlockTimeout = {FTX_DEADLOCK_TIMEOUT_SEC, FTX_DEADLOCK_TIMEOUT_NSEC};

    int32_t state;
    int32_t oldSeq = c->seq;

    if (c->m != m)
    {
        // Atomically assign the mutex to our condition.
        assert(c->m == NULL);
        ftx_mutex * old = __sync_val_compare_and_swap(&(c->m), 0, m);
        assert(old == NULL);
    }

    ftx_mutex_unlock(c->m);

    // Wait for a signal.
    // The sequence number will change from the current sequence number.
    int errorCode = syscall(SYS_futex, &(c->seq), FUTEX_WAIT, oldSeq, timeout, NULL, 0);

    assert(errorCode == 0 || (errorCode == -1 && (errno == EAGAIN || errno == ETIMEDOUT)));
    if (errorCode == -1 && errno == ETIMEDOUT)
    {
        errorCode = ftx_mutex_timed_lock(c->m, &deadlockTimeout);
        if (errorCode == ETIMEDOUT)
            return EDEADLOCK;
        return ETIMEDOUT;
    }

    // The mutex must be changed to the contended state.
    for(;;)
    {
        state = __sync_lock_test_and_set(c->m, FTX_MUTEX_CONTESTED);

        // It's our lock.
        // Transition FREE -> CONTESTED.
        if (state == FTX_MUTEX_FREE)
            return 0;

        // Wait for the mutex to change from the contested state.
        // Transition LOCKED -> CONTESTED.
        int errorCode = syscall(SYS_futex, c->m, FUTEX_WAIT, FTX_MUTEX_CONTESTED, &deadlockTimeout, NULL, 0);
        assert(errorCode == 0 || (errorCode == -1 && (errno == EAGAIN || errno == ETIMEDOUT)));
        if (errorCode == -1 && errno == ETIMEDOUT)
            return EDEADLOCK;
    }
}

// This function waits for the condition to change for a set duration. An
// EDEADLOCK error is returned if the condition mutex is locked for more
// than FTX_DEADLOCK_TIMEOUT_SEC seconds.
inline int ftx_cond_wait(ftx_condition *c, ftx_mutex *m)
{
    return ftx_cond_timed_wait(c, m, NULL);
}

#endif	/* FTX_SYNC_HPP */
