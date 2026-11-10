#include "table.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <system_error>

namespace {
void Check(int status, const char *operation) {
    if (status != 0) { fprintf(stderr, "%s failed: %d\n", operation, status); abort(); }
}
int ValidCount(int n) {
    if (n < 2) throw std::invalid_argument("at least two philosophers are required");
    return n;
}
}

Table::Table(int n) : participants(ValidCount(n)), occupied(participants, false),
    state(participants, 0), head(NULL), tail(NULL), currentTurn(0) {
    int status = pthread_mutex_init(&mutex, NULL);
    if (status != 0) throw std::system_error(status, std::generic_category(), "table mutex init");
    status = pthread_cond_init(&cond, NULL);
    if (status != 0) {
        pthread_mutex_destroy(&mutex);
        throw std::system_error(status, std::generic_category(), "table condition init");
    }
}
void Table::validate(int id) const {
    if (id < 0 || id >= participants) throw std::out_of_range("philosopher id");
}
void Table::remove(Request *request) {
    if (request->previous) request->previous->next = request->next;
    else head = request->next;
    if (request->next) request->next->previous = request->previous;
    else tail = request->previous;
    request->queued = false;
}
void Table::FinishWait(void *arg) {
    Request *request = static_cast<Request*>(arg);
    Table *table = request->table;
    // cond_wait reacquires the mutex before cancellation invokes this handler.
    if (request->queued) {
        table->remove(request);
        table->state[request->id] = 0;
        Check(pthread_cond_broadcast(&table->cond), "queue cancellation wakeup");
    }
    Check(pthread_mutex_unlock(&table->mutex), "table wait unlock");
}
void Table::wait(int id) {
    validate(id);
    Check(pthread_mutex_lock(&mutex), "table lock");
    assert(state[id] == 0); // At most one outstanding request per philosopher.
    Request request = {this, id, tail, NULL, true};
    if (tail) tail->next = &request;
    else head = &request;
    tail = &request;
    state[id] = 1;
    const int right = (id + 1) % participants;
    pthread_cleanup_push(FinishWait, &request);
    // No bypass: only the oldest request can reserve its pair. Once granted,
    // a disjoint next request may proceed while this meal is still active.
    while (head != &request || occupied[id] || occupied[right])
        Check(pthread_cond_wait(&cond, &mutex), "table wait");
    remove(&request);
    occupied[id] = occupied[right] = true;
    state[id] = 2;
    Check(pthread_cond_broadcast(&cond), "queue grant wakeup");
    pthread_cleanup_pop(1);
}
void Table::signal(int id) {
    validate(id);
    Check(pthread_mutex_lock(&mutex), "table lock");
    assert(state[id] == 2);
    occupied[id] = occupied[(id + 1) % participants] = false;
    state[id] = 0;
    Check(pthread_cond_broadcast(&cond), "pair release wakeup");
    Check(pthread_mutex_unlock(&mutex), "table unlock");
}
int Table::getCurrentTurn() {
    Check(pthread_mutex_lock(&mutex), "turn read lock");
    int turn = currentTurn;
    Check(pthread_mutex_unlock(&mutex), "turn read unlock");
    return turn;
}
void Table::updateCurrentTurn() {
    Check(pthread_mutex_lock(&mutex), "turn update lock");
    currentTurn = (currentTurn + 1) % participants;
    Check(pthread_mutex_unlock(&mutex), "turn update unlock");
}
Table::~Table() {
    assert(head == NULL && tail == NULL);
    for (int id = 0; id < participants; ++id) assert(state[id] == 0);
    Check(pthread_cond_destroy(&cond), "table condition destroy");
    Check(pthread_mutex_destroy(&mutex), "table mutex destroy");
}
