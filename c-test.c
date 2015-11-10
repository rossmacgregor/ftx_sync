//
// c-test.c
//
// Copyright (c) 2015 Ross MacGregor
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "ftx_sync.h"

static ftx_mutex m;
static int counter;
static int lastCount;
static int incrementValue;
static int done;
static ftx_condition c;

void *readerProc(void *arg)
{
    (void)arg;
    struct timespec timeout;
    timeout.tv_sec = 3;
    timeout.tv_nsec = 0;

    ftx_mutex_lock(&m);

    while(!done)
    {
        int rc = ftx_cond_timed_wait(&c, &m, &timeout);

        if (rc == EDEADLOCK)
        {
            printf("Reader Deadlock\n");
            break;
        }

        if (rc == ETIMEDOUT)
        {
            printf("Reader Timeout\n");
            continue;
        }

        printf("Counter=%d\n", counter);
        
        if (counter > 30)
            done = 1;
    }

    ftx_mutex_unlock(&m);

    return 0;
}

void *writerProc(void *arg)
{
    (void)arg;
    while(!done)
    {
        sleep(1);

        int rc = ftx_mutex_lock(&m);
        if (rc == EDEADLOCK)
        {
            printf("Writer Deadlock\n");
            break;
        }
        
        counter = counter + incrementValue;

        ftx_mutex_unlock(&m);

        ftx_cond_signal(&c);
    }

    return 0;
}

void *badWriterProc(void *arg)
{
    (void)arg;
    sleep(5);

    ftx_mutex_lock(&m);

    counter = counter + incrementValue;

    return 0;
}


void *incProc(void *arg)
{
    (void)arg;
    struct timespec timeout;
    timeout.tv_sec = 5;
    timeout.tv_nsec = 0;

    sleep(1);

    for(int i=0; i<100; ++i)
    {
        int error = ftx_mutex_timed_lock(&m, &timeout);
        if (error)
        {
            printf("lock timed out\n");
            break;
        }

        lastCount = counter;
        incrementValue = 1;
        usleep(10 * 1000);
        counter = lastCount + incrementValue;
        incrementValue = 5;

        ftx_mutex_unlock(&m);
    }

    return 0;
}

int main(int argc, char** argv)
{
    incrementValue = 1;

    ftx_mutex_init(&m);
    ftx_cond_init(&c);

    // Test mutex
    //
    printf("Testing mutex...\n");
    
    pthread_t incrementThread1;
    pthread_t incrementThread2;
    pthread_t incrementThread3;
    pthread_t incrementThread4;
    pthread_t incrementThread5;
    pthread_t incrementThread6;
    pthread_t incrementThread7;
    pthread_t incrementThread8;
    pthread_t incrementThread9;
    pthread_t incrementThread10;

    pthread_create(&incrementThread1, NULL, incProc, 0);
    pthread_create(&incrementThread2, NULL, incProc, 0);
    pthread_create(&incrementThread3, NULL, incProc, 0);
    pthread_create(&incrementThread4, NULL, incProc, 0);
    pthread_create(&incrementThread5, NULL, incProc, 0);
    pthread_create(&incrementThread6, NULL, incProc, 0);
    pthread_create(&incrementThread7, NULL, incProc, 0);
    pthread_create(&incrementThread8, NULL, incProc, 0);
    pthread_create(&incrementThread9, NULL, incProc, 0);
    pthread_create(&incrementThread10, NULL, incProc, 0);

    pthread_join(incrementThread1, NULL);
    pthread_join(incrementThread2, NULL);
    pthread_join(incrementThread3, NULL);
    pthread_join(incrementThread4, NULL);
    pthread_join(incrementThread5, NULL);
    pthread_join(incrementThread6, NULL);
    pthread_join(incrementThread7, NULL);
    pthread_join(incrementThread8, NULL);
    pthread_join(incrementThread9, NULL);
    pthread_join(incrementThread10, NULL);

    printf("counter=%d\n", counter);
    
    assert(counter == 1000);

    // Test condition variable
    //
    counter = 0;
    incrementValue = 1;
    done = 0;
    printf("Testing condition variable...\n");
    pthread_t readerThread;
    pthread_t writerThread1;
    pthread_t writerThread2;
    pthread_t writerThread3;
    pthread_t writerThread4;
    pthread_create(&readerThread, NULL, readerProc, 0);
    pthread_create(&writerThread1, NULL, writerProc, 0);
    pthread_create(&writerThread2, NULL, writerProc, 0);
    pthread_create(&writerThread3, NULL, writerProc, 0);

    pthread_join(readerThread, NULL);
    pthread_join(writerThread1, NULL);
    pthread_join(writerThread2, NULL);
    pthread_join(writerThread3, NULL);

    // Test error handling
    //
    counter = 0;
    incrementValue = 1;
    done = 0;
    printf("Testing deadlock...\n");
    pthread_create(&readerThread, NULL, readerProc, 0);
    pthread_create(&writerThread1, NULL, writerProc, 0);
    pthread_create(&writerThread2, NULL, writerProc, 0);
    pthread_create(&writerThread3, NULL, writerProc, 0);
    pthread_create(&writerThread4, NULL, badWriterProc, 0);

    pthread_join(readerThread, NULL);
    pthread_join(writerThread1, NULL);
    pthread_join(writerThread2, NULL);
    pthread_join(writerThread3, NULL);
    pthread_join(writerThread4, NULL);
    
    return 0;
}
