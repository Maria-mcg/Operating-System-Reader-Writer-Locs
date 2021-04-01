#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <ctype.h>

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

long long do_work() {

    long long n= (long long)rand();
    long long value=0;
    for(int i=0; i < LOOP; ++i)
        for(int j=0; j < LOOP; ++j)
            if(value % 2 == 0)
                value += i + j + n;
            else
                value -= i - j + n;

    return value;
}

size_t count_letter(char * buff, size_t length, char letter, bool ignore_case) {

    size_t counter= 0;
    for(size_t i=0; i < length; ++i) {
        if(buff[i] == '\0')
            break;

        if(ignore_case) {
            if(tolower(buff[i]) == tolower(letter))
                ++counter;
        } else if(buff[i] == letter)
            ++counter;
    }

    return counter;
}

void rwlock_init(rwlock_t *rw, size_t num_writers, size_t num_readers) {
    rw->active_readers= 0;
    rw->pending_readers= num_readers;
    rw->initial_readers= num_readers;
    rw->pending_writers= num_writers;
    rw->initial_writers= num_writers;

    sem_init(&rw->read_mutex, 0, 1);
    sem_init(&rw->write_mutex, 0, 1);
    sem_init(&rw->pending_rw_lock, 0, 1);
    sem_init(&rw->balancer_0, 0, 0);
    sem_init(&rw->balancer_1, 0, 0);
}

// readers_writers_string: "rrrwwwrwrww"
void rwlock_init_rwstr(rwlock_t *rw, char * readers_writers_string, size_t length) {
    rw->active_readers= 0;
    rw->pending_readers= count_letter(readers_writers_string, length, 'R', true);
    rw->pending_writers= count_letter(readers_writers_string, length, 'W', true);
    rw->initial_readers= rw->pending_readers;
    rw->initial_writers= rw->pending_writers;

    sem_init(&rw->read_mutex, 0, 1);
    sem_init(&rw->write_mutex, 0, 1);
    sem_init(&rw->pending_rw_lock, 0, 1);
    sem_init(&rw->balancer_0, 0, 0);
    sem_init(&rw->balancer_1, 0, 0);
}

void rwlock_destroy(rwlock_t *rw) {
    sem_destroy(&rw->read_mutex);
    sem_destroy(&rw->write_mutex);
    sem_destroy(&rw->balancer_0);
    sem_destroy(&rw->balancer_1);
    sem_destroy(&rw->pending_rw_lock);
}

void rwlock_acquire_readlock(thread_info * th_info) {

    sem_wait(&th_info->rw->pending_rw_lock);
    float percentage_remaining_writers= (th_info->rw->pending_writers / (float) th_info->rw->initial_writers) * 100.00;
    float percentage_remaining_readers= (th_info->rw->pending_readers / (float) th_info->rw->initial_readers) * 100.00;

    printf("[reader] thread: %u\tstatus: checking-balance\tpending-readers: %d (%f%%)\tpending-writers: %d (%f%%)\n", th_info->thread_id, th_info->rw->pending_readers, percentage_remaining_readers, th_info->rw->pending_writers, percentage_remaining_writers);
    if(percentage_remaining_readers <= percentage_remaining_writers && th_info->rw->pending_writers > 0) {
        printf("[reader] thread: %u\twaiting balancer_0... (giving chance for writers as their percentage is higher or equal pending-readers: %d (%f%%) pending-writers: %d (%f%%))\n", th_info->thread_id, th_info->rw->pending_readers, percentage_remaining_readers, th_info->rw->pending_writers, percentage_remaining_writers);
        sem_post(&th_info->rw->pending_rw_lock);

        sem_wait(&th_info->rw->balancer_0);
        printf("[reader] thread: %u\twaiting balancer_0... done\n", th_info->thread_id);
    } else
        sem_post(&th_info->rw->pending_rw_lock);

    sem_wait(&th_info->rw->read_mutex);

    if (th_info->rw->active_readers == 0)
        sem_wait(&th_info->rw->write_mutex); // first reader acquires writelock

    printf("[reader] thread: %u\tstatus: running-stage-0\tnum-readers: %d\tshared-resource: %lld (incrementing active-readers...)\n", th_info->thread_id, th_info->rw->active_readers, *(th_info->shared_resource));
    th_info->rw->active_readers++;
    printf("[reader] thread: %u\tstatus: running-stage-0\tnum-readers: %d\tshared-resource: %lld (incrementing active-readers...done)\n", th_info->thread_id, th_info->rw->active_readers, *(th_info->shared_resource));

    sem_post(&th_info->rw->read_mutex);
}

void rwlock_release_readlock(thread_info * th_info) {

    sem_wait(&th_info->rw->read_mutex);

    printf("[reader] thread: %u\tstatus: release-stage-0\tnum-readers: %d\tshared-resource: %lld (decrementing active-readers...)\n", th_info->thread_id, th_info->rw->active_readers, *(th_info->shared_resource));
    th_info->rw->active_readers--;
    printf("[reader] thread: %u\tstatus: release-stage-0\tnum-readers: %d\tshared-resource: %lld (decrementing active-readers...done)\n", th_info->thread_id, th_info->rw->active_readers, *(th_info->shared_resource));

    sem_wait(&th_info->rw->pending_rw_lock);
    printf("[reader] thread: %u\tstatus: release-stage-1\tnum-readers: %d\tshared-resource: %lld pending-readers: %d (decrementing pending-readers...done)\n", th_info->thread_id, th_info->rw->active_readers, *(th_info->shared_resource), th_info->rw->pending_readers);
    th_info->rw->pending_readers--;
    printf("[reader] thread: %u\tstatus: release-stage-1\tnum-readers: %d\tshared-resource: %lld pending-readers: %d (decrementing pending-readers...done)\n", th_info->thread_id, th_info->rw->active_readers, *(th_info->shared_resource), th_info->rw->pending_readers);

    float percentage_remaining_writers= (th_info->rw->pending_writers / (float) th_info->rw->initial_writers) * 100.00;
    float percentage_remaining_readers= (th_info->rw->pending_readers / (float) th_info->rw->initial_readers) * 100.00;
    printf("[reader] thread: %u\tstatus: release-stage-1\tpending-readers: %d (%f%%)\tpending-writers: %d (%f%%) (checking balance) \n", th_info->thread_id, th_info->rw->pending_readers, percentage_remaining_readers, th_info->rw->pending_writers, percentage_remaining_writers);

    if(th_info->rw->pending_readers == 0) {
        printf("[reader] thread: %u\tstatus: release-stage-1 (freeing balancer_1...)\n", th_info->thread_id);
        for(int i=0; i < th_info->rw->initial_readers; ++i)
            sem_post(&th_info->rw->balancer_1);
        printf("[reader] thread: %u\tstatus: release-stage-1 (freeing balancer_1...done)\n", th_info->thread_id);
    }
    else if(percentage_remaining_readers <= percentage_remaining_writers && th_info->rw->pending_writers > 0) {
        printf("[reader] thread: %u\tstatus: checking-balance\tpending-readers: %d (%f%%)\tpending-writers: %d (%f%%) (giving chance for writers as their percentage is higher or equal)\n", th_info->thread_id, th_info->rw->pending_readers, percentage_remaining_readers, th_info->rw->pending_writers, percentage_remaining_writers);
        sem_post(&th_info->rw->balancer_1);// give chance for writers as their percentage is higher
    }
    else {
        printf("[reader] thread: %u\tstatus: checking-balance\tpending-readers: %d (%f%%)\tpending-writers: %d (%f%%) (giving chance for readers as their percentage is higher)\n", th_info->thread_id, th_info->rw->pending_readers, percentage_remaining_readers, th_info->rw->pending_writers, percentage_remaining_writers);
        sem_post(&th_info->rw->balancer_0); // give chance for readers as their percentage is higher
    }

    sem_post(&th_info->rw->pending_rw_lock);

    if (th_info->rw->active_readers == 0)
        sem_post(&th_info->rw->write_mutex); // last reader releases writelock

    printf("[reader] thread: %u\tstatus: release-stage-1 (halting thread....)\n", th_info->thread_id);
    sem_post(&th_info->rw->read_mutex);
    printf("[reader] thread: %u\tstatus: release-stage-1 (halting thread....)\n", th_info->thread_id);
}

void rwlock_acquire_writelock(thread_info * th_info) {

    printf("[writer] thread: %u\twaiting write-lock...\n", th_info->thread_id);
    sem_wait(&th_info->rw->write_mutex);
    printf("[writer] thread: %u\twaiting write-lock...done\n", th_info->thread_id);

    sem_wait(&th_info->rw->pending_rw_lock);
    float percentage_remaining_writers= (th_info->rw->pending_writers / (float) th_info->rw->initial_writers) * 100.00;
    float percentage_remaining_readers= (th_info->rw->pending_readers / (float) th_info->rw->initial_readers) * 100.00;
    printf("[writer] thread: %u\tstatus: checking-balance\tpending-readers: %d (%f%%)\tpending-writers: %d (%f%%)\n", th_info->thread_id, th_info->rw->pending_readers, percentage_remaining_readers, th_info->rw->pending_writers, percentage_remaining_writers);
    if(percentage_remaining_writers < percentage_remaining_readers && th_info->rw->pending_readers > 0) {
        printf("[writer] thread: %u\twaiting balancer_1... (giving chance for readers as their percentage is higher pending-readers: %d (%f%%) pending-writers: %d (%f%%))\n", th_info->thread_id, th_info->rw->pending_readers, percentage_remaining_readers, th_info->rw->pending_writers, percentage_remaining_writers);
        sem_post(&th_info->rw->pending_rw_lock);
        sem_post(&th_info->rw->write_mutex);
        sem_wait(&th_info->rw->balancer_1);
        sem_wait(&th_info->rw->write_mutex);
        printf("[writer] thread: %u\twaiting balancer_1... done\n", th_info->thread_id);
    } else
        sem_post(&th_info->rw->pending_rw_lock);
}

void rwlock_release_writelock(thread_info * th_info) {

    sem_wait(&th_info->rw->pending_rw_lock);
    printf("[writer] thread: %u\tstatus: release-stage-0\tshared-resource: %lld (decrementing pending writers...)\n", th_info->thread_id, *(th_info->shared_resource));
    th_info->rw->pending_writers--;
    printf("[writer] thread: %u\tstatus: release-stage-0\tshared-resource: %lld (decrementing pending writers...done)\n", th_info->thread_id, *(th_info->shared_resource));

    if(th_info->rw->pending_writers == 0) {
        printf("[writer] thread: %u\tstatus: release-stage-1 (freeing balancer_0...)\n", th_info->thread_id);
        for(int i=0; i < th_info->rw->initial_readers; ++i)
            sem_post(&th_info->rw->balancer_0);
        printf("[writer] thread: %u\tstatus: release-stage-1 (freeing balancer_0...done)\n", th_info->thread_id);
    }
    else
        sem_post(&th_info->rw->balancer_0);

    sem_post(&th_info->rw->pending_rw_lock);

    printf("[writer] thread: %u\tstatus: release-stage-1 (halting thread....)\n", th_info->thread_id);
    sem_post(&th_info->rw->write_mutex);
    printf("[writer] thread: %u\tstatus: release-stage-1 (halting thread...done)\n", th_info->thread_id);
}

void* rwlock_run_reader(void *arg) {
    thread_info *th= (thread_info *)arg;

    rwlock_acquire_readlock(th);
    printf("[reader] thread: %u\tdo_work()...\n", th->thread_id);
    long long work_result= do_work();
    printf("[reader] thread: %u\tdo_work()...done\n", th->thread_id);
    printf("[reader] thread: %u\tstatus: running\t\tnum-readers: %d shared-resource: %lld work-result: %lld\n", th->thread_id, th->rw->active_readers, *(th->shared_resource), work_result);
    rwlock_release_readlock(th);
}

void* rwlock_run_writer(void *arg) {
    thread_info *th_info= (thread_info *)arg;

    printf("[writer] thread: %u\twaiting write-lock...\n", th_info->thread_id);
    rwlock_acquire_writelock(th_info);
    printf("[writer] thread: %u\tdo_work()...\n", th_info->thread_id);
    long long work_result= do_work();
    printf("[writer] thread: %u\tdo_work()...done\n", th_info->thread_id);
    printf("[writer] thread: %u\tstatus: running shared-resource: %lld work-result: %lld (updating shared resource...)\n", th_info->thread_id, *(th_info->shared_resource), work_result);
    (*(th_info->shared_resource))++;
    printf("[writer] thread: %u\tstatus: running shared-resource: %lld work-result: %lld (updating shared resource...done)\n", th_info->thread_id, *(th_info->shared_resource), work_result);
    rwlock_release_writelock(th_info);
}
