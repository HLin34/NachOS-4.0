#include "philosopher.hpp"


Philosopher::Philosopher(int id, Fork *leftFork, Fork *rightFork, Table *table) :id(id), cancelled(false), leftFork(leftFork), rightFork(rightFork), table(table) {
    srand((unsigned) time(&t1));
}

void Philosopher::start() {
    // TODO: start a philosopher thread
    pthread_create(&t, NULL, &Philosopher::run, this);      // Create a new thread, starting with execution of START-ROUTINE
                                                            // getting passed ARG. Creation attributed come from ATTR. The new
                                                            // handle is stored in *NEWTHREAD.
}

int Philosopher::join() {
    // TODO: join a philosopher thread
    return pthread_join(t, NULL);                           // Make calling thread wait for termination of the thread TH. The
                                                            // exit status of the thread is stored in *THREAD_RETURN, if THREAD_RETURN
                                                            // is not NULL.
}

int Philosopher::cancel() {
    // TODO: cancel a philosopher thread
    cancelled = true;
    return pthread_cancel(t);                               // Cancel THREAD immediately or at the next possibility
}

void Philosopher::think() {
    int thinkTime = rand() % MAXTHINKTIME + MINTHINKTIME;
    sleep(thinkTime);
    printf("Philosopher %d is thinking for %d seconds.\n", id, thinkTime);
}

void Philosopher::eat() {
    printf("Philosopher %d is eating.\n", id);
    sleep(EATTIME);
}

void Philosopher::pickup(int id) {
    // TODO: implement the pickup interface, the philosopher needs to pick up the left fork first, then the right fork
    //printf("Philosopher %d is trying to pick up forks.\n", id);

    leftFork->wait();
    //printf("Philosopher %d picked up left fork.\n", id);
    rightFork->wait();
    //printf("Philosopher %d picked up right fork.\n", id);
    printf("Philosopher %d picked up both left and right fork.\n", id );
    //table->updateCurrentTurn();                       // Starvation
}

void Philosopher::putdown(int id) {
    // TODO: implement the putdown interface, the philosopher needs to put down the left fork first, then the right fork
    leftFork->signal();
    //printf("Philosopher %d put down left fork.\n", id );
    rightFork->signal();
    //printf("Philosopher %d put down right fork.\n", id );
    printf("Philosopher %d put down both left and right fork.\n", id );
}

void Philosopher::enter() {
    // TODO: implement the enter interface, the philosopher needs to join the table first
    table->wait();
    printf("Philosopher %d enter the table.\n", id);
}

void Philosopher::leave() {
    // TODO: implement the leave interface, the philosopher needs to let the table know that he has left
    table->signal();
    printf("Philosopher %d leave the table.\n", id);
}

void* Philosopher::run(void* arg) {
    // TODO: complete the philosopher thread routine. 
    Philosopher* p = static_cast<Philosopher*>(arg);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);   // Set cancellation state of current thread to TYPE, returning the old
                                                            // type in *OLDTYPE if OLDTYPE is not NULL.

    while (!p->cancelled) {
        p->think();
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        
        p->enter();
        printf("Now is %d's turn.\n", p->table->getCurrentTurn());

        if (p->table->getCurrentTurn() == p->id || ((p->table->getCurrentTurn()+1)%5 != p->id  && (p->table->getCurrentTurn()-1)%5 != p->id)) {
            //printf("Now is %d's turn.\n", p->table->getCurrentTurn());
            p->pickup(p->id); // Attempt to pick up forks if it's the philosopher's turn

            p->eat();

            p->putdown(p->id); // Put down forks
            
        }
        //p->table->wait();          // Starvation
        /*p->pickup(p->id);
        p->eat();
        p->putdown(p->id);*/
        //p->table->signal();         // Starvation
        //p->table->updateCurrentTurn();
        p->leave();

        p->table->updateCurrentTurn();

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
    

    return NULL;
}