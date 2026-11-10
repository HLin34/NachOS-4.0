#include "philosopher.hpp"
#include <system_error>

static_assert(PHILOSOPHERS >= 2, "at least two philosophers are required");
static_assert(MINTHINKTIME >= 0 && MAXTHINKTIME >= MINTHINKTIME, "invalid thinking interval");
static_assert(EATTIME >= 0 && RUNTIME >= 0, "invalid runtime");

namespace {
struct Meal {
    Fork *left;
    Fork *right;
    Table *table;
    int id;
    bool seated;
    bool forksHeld;
};
void ReleaseFork(void *arg) { static_cast<Fork*>(arg)->signal(); }
void ReleaseMeal(void *arg) {
    Meal *meal = static_cast<Meal*>(arg);
    if (meal->forksHeld) {
        meal->left->signal();
        meal->right->signal();
    }
    if (meal->seated) meal->table->signal(meal->id);
}
}

Philosopher::Philosopher(int id, Fork *leftFork, Fork *rightFork, Table *table)
    : id(id), cancelled(false), leftFork(leftFork), rightFork(rightFork), table(table) {
    // Reuse existing per-object storage for a worker-owned RNG state.
    t1 = static_cast<unsigned int>(time(NULL)) ^ (static_cast<unsigned int>(id + 1) * 2654435761U);
}
void Philosopher::start() {
    int status = pthread_create(&t, NULL, &Philosopher::run, this);
    if (status != 0) throw std::system_error(status, std::generic_category(), "philosopher start");
}
int Philosopher::join() { return pthread_join(t, NULL); }
int Philosopher::cancel() {
    // A deferred cancellation request is the stop mechanism; no shared bool.
    return pthread_cancel(t);
}
void Philosopher::think() {
    unsigned int state = static_cast<unsigned int>(t1);
    int thinkTime = rand_r(&state) % (MAXTHINKTIME - MINTHINKTIME + 1) + MINTHINKTIME;
    t1 = state;
    sleep(thinkTime);
    printf("Philosopher %d is thinking for %d seconds.\n", id, thinkTime);
}
void Philosopher::eat() {
    printf("Philosopher %d is eating.\n", id);
    sleep(EATTIME);
}
void Philosopher::pickup(int id) {
    (void)id;
    leftFork->wait();
    // If cancellation occurs waiting for the right fork, return the left one.
    pthread_cleanup_push(ReleaseFork, leftFork);
    rightFork->wait();
    pthread_cleanup_pop(0);
}
void Philosopher::putdown(int id) {
    (void)id;
    leftFork->signal();
    rightFork->signal();
}
void Philosopher::enter() { table->wait(id); }
void Philosopher::leave() { table->signal(id); }

void *Philosopher::run(void *arg) {
    Philosopher *p = static_cast<Philosopher*>(arg);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    for (;;) {
        pthread_testcancel();
        p->think();
        Meal meal = {p->leftFork, p->rightFork, p->table, p->id, false, false};
        pthread_cleanup_push(ReleaseMeal, &meal);
        p->enter();
        meal.seated = true;
        // Table has already reserved both forks for this request.
        p->pickup(p->id);
        meal.forksHeld = true;
        p->eat();
        p->putdown(p->id);
        meal.forksHeld = false;
        p->leave();
        meal.seated = false;
        p->table->updateCurrentTurn();
        pthread_cleanup_pop(0);
    }
    return NULL;
}
