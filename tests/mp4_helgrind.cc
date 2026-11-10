// Independent checker driver: no atomics/interposition except accelerated sleep.
#include "philosopher.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" unsigned __wrap_sleep(unsigned) { usleep(500); return 0; }
static void Check(bool value) { if(!value) abort(); }
static void *Turns(void *arg) {
    Table *table=static_cast<Table*>(arg);
    for(int i=0;i<1000;i++) { table->updateCurrentTurn(); table->getCurrentTurn(); }
    return NULL;
}
int main(int argc,char **argv) {
    Check(argc==2);
    Table *table=new Table(PHILOSOPHERS);
    Fork *forks[PHILOSOPHERS];
    for(int i=0;i<PHILOSOPHERS;i++) forks[i]=new Fork;
    if(strcmp(argv[1],"turn_race")==0) {
        pthread_t threads[4];
        for(int i=0;i<4;i++) Check(pthread_create(&threads[i],NULL,Turns,table)==0);
        for(int i=0;i<4;i++) Check(pthread_join(threads[i],NULL)==0);
        Check(table->getCurrentTurn()==4000%PHILOSOPHERS);
    } else {
        Philosopher *workers[PHILOSOPHERS];
        for(int i=0;i<PHILOSOPHERS;i++) workers[i]=new Philosopher(i,forks[i],forks[(i+1)%PHILOSOPHERS],table);
        for(int i=0;i<PHILOSOPHERS;i++) workers[i]->start();
        usleep(100000);
        for(int i=PHILOSOPHERS-1;i>=0;i--) Check(workers[i]->cancel()==0);
        for(int i=0;i<PHILOSOPHERS;i++) Check(workers[i]->join()==0);
        for(int i=0;i<PHILOSOPHERS;i++) delete workers[i];
    }
    for(int i=0;i<PHILOSOPHERS;i++) { forks[i]->wait(); forks[i]->signal(); delete forks[i]; }
    for(int i=0;i<PHILOSOPHERS;i++) { table->wait(i); table->signal(i); }
    delete table;
    puts("PASS"); return 0;
}
