#ifndef RWMAIN_READERWRITER_H
#define RWMAIN_READERWRITER_H

#include <pthread.h>
#include <semaphore.h>

#define LOOP 1000

typedef struct _rwlock_t {
    sem_t read_mutex; // binary semaphore
    sem_t write_mutex; // binary semaphore
    sem_t pending_rw_lock; // binary semaphore
    sem_t balancer_0;
    sem_t balancer_1;

    size_t active_readers;
    size_t pending_writers;
    size_t pending_readers;
    size_t initial_readers;
    size_t initial_writers;
} rwlock_t;

typedef struct _thread_info {
    rwlock_t *rw;
    long long *shared_resource;
    pthread_t thread_id;
} thread_info;

long long do_work();
size_t count_letter(char * buff, size_t length, char letter, bool ignore_case);
void rwlock_init(rwlock_t *rw, size_t num_writers, size_t num_readers);
void rwlock_init_rwstr(rwlock_t *rw, char * readers_writers_string, size_t length);
void rwlock_destroy(rwlock_t *rw);
void rwlock_acquire_readlock(thread_info * th_info);
void rwlock_release_readlock(thread_info * th_info);
void rwlock_acquire_writelock(thread_info * th_info);
void rwlock_release_writelock(thread_info * th_info);
void* rwlock_run_reader(void *arg);
void* rwlock_run_writer(void *arg);

#endif //RWMAIN_READERWRITER_H
