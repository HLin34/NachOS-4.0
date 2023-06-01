#include "fork.hpp"

Fork::Fork() {
    // TODO: implement fork constructor (value, mutex, cond)
    value = 1;                               // means that this fork is able to be used
    pthread_mutex_init(&mutex, NULL);        // initialize a mutex
    pthread_cond_init(&cond, NULL);          // initialize a condition variable
}

void Fork::wait() {
    // TODO: implement semaphore wait
    pthread_mutex_lock(&mutex);              // enter critical section, lock mutex
    while(value != 1){
        pthread_cond_wait(&cond, &mutex);    // Wait for condition variable COND to be signaled or broadcast. 
                                             // MUTEX is assumed to be locked before.
    }
    value = 0;                               // means that this fork is not able to be used
    pthread_mutex_unlock(&mutex);            // leave critical section, release the lock
}

void Fork::signal() {
    // TODO: implement semaphore signal
    pthread_mutex_lock(&mutex);              // enter critical section, lock mutex
    value = 1;                               // means that this fork is able to be used
    pthread_cond_signal(&cond);              // Wake up one thread waiting for condition variable
    pthread_mutex_unlock(&mutex);            // leave critical section, release the lock
}

Fork::~Fork() {
    // TODO: implement fork destructor (mutex, cond)
    pthread_mutex_destroy(&mutex);           // destroy a mutex
    pthread_cond_destroy(&cond);             // destroy a condition variable
}