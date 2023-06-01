#ifndef PHILOSOPHER_HPP 
#define PHILOSOPHER_HPP 

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "fork.hpp"
#include "table.hpp"
#include "config.hpp"

class Philosopher { 
public: 
    Philosopher(int id, Fork *leftFork, Fork *rightFork, Table *table);
    void start();
    int join();
    int cancel();
    void think();
    void enter();
    void pickup(int id);
    void eat();
    void putdown(int id);
    void leave();

private:
    static void* run(void* arg);
    int id;
    bool cancelled;
    Fork *leftFork, *rightFork;
    Table *table;
    pthread_t t;
    time_t t1;
};


#endif // Philosopher_Hpp