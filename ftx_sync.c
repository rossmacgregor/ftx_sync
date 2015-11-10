//
// ftx_sync.c
//
// Copyright (c) 2015 Ross MacGregor
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "ftx_sync.h"

extern inline void ftx_mutex_init(ftx_mutex *m);
extern inline int ftx_mutex_try_lock(ftx_mutex *m);
extern inline int ftx_mutex_timed_lock(ftx_mutex *m, const struct timespec *timeout);
extern inline int ftx_mutex_lock(ftx_mutex *m);
extern inline void ftx_mutex_unlock(ftx_mutex *m);
extern inline void ftx_cond_init(ftx_condition *c);
extern inline void ftx_cond_signal(ftx_condition *c);
extern inline void ftx_cond_broadcast(ftx_condition *c);
extern inline int ftx_cond_timed_wait(ftx_condition *c, ftx_mutex *m, const struct timespec * timeout);
extern inline int ftx_cond_wait(ftx_condition *c, ftx_mutex *m);
