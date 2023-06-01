#include "table.hpp"
#include "stdio.h"
#include "config.hpp"

Table::Table(int n) {
    // TODO: implement table constructor (value, mutex, cond)
    currentTurn = 0;                          // Starvation
    value = 4;                                // means that the table can contain n people                        
    pthread_mutex_init(&mutex, NULL);         // initialize a mutex
    pthread_cond_init(&cond, NULL);           // initialize a condition variable
}

void Table::wait() {
    // TODO: implement semaphore wait
    pthread_mutex_lock(&mutex);               // enter critical section, lock mutex
    while(value <= 0){
        pthread_cond_wait(&cond, &mutex);     // Wait for condition variable COND to be signaled or broadcast. 
                                              // MUTEX is assumed to be locked before.
    }
    value--;                                  // means that the amount of people the table can contain decline 1                    
    pthread_mutex_unlock(&mutex);             // leave critical section, release the lock
}

void Table::signal() {
    // TODO: implement semaphore signal
    pthread_mutex_lock(&mutex);               // enter critical section, lock mutex
    value++;                                  // means that the amount of people the table can contain increase 1
    pthread_cond_signal(&cond);               // Wake up one thread waiting for condition variable
    pthread_mutex_unlock(&mutex);             // leave critical section, release the lock
}

int Table::getCurrentTurn(){                  // Starvation
    return currentTurn;
}

void Table::updateCurrentTurn(){              // Starvation
    currentTurn = (currentTurn + 1) % PHILOSOPHERS;
    printf("update to %d's turn.\n", currentTurn);
}

Table::~Table() {
    // TODO: implement table destructor (mutex, cond)
    pthread_mutex_destroy(&mutex);            // destroy a mutex
    pthread_cond_destroy(&cond);              // destroy a condition variable
}