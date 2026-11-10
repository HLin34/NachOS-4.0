#include "fork.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <system_error>

static void Check(int status, const char *operation) {
    if (status != 0) { fprintf(stderr, "%s failed: %d\n", operation, status); abort(); }
}
static void Unlock(void *mutex) {
    Check(pthread_mutex_unlock(static_cast<pthread_mutex_t*>(mutex)), "fork unlock");
}

Fork::Fork() : value(1) {
    int status = pthread_mutex_init(&mutex, NULL);
    if (status != 0) throw std::system_error(status, std::generic_category(), "fork mutex init");
    status = pthread_cond_init(&cond, NULL);
    if (status != 0) {
        pthread_mutex_destroy(&mutex);
        throw std::system_error(status, std::generic_category(), "fork condition init");
    }
}
void Fork::wait() {
    Check(pthread_mutex_lock(&mutex), "fork lock");
    // cond_wait reacquires mutex before cancellation cleanup runs.
    pthread_cleanup_push(Unlock, &mutex);
    while (value != 1) Check(pthread_cond_wait(&cond, &mutex), "fork wait");
    value = 0;
    pthread_cleanup_pop(1);
}
void Fork::signal() {
    Check(pthread_mutex_lock(&mutex), "fork lock");
    assert(value == 0);
    value = 1;
    Check(pthread_cond_signal(&cond), "fork signal");
    Check(pthread_mutex_unlock(&mutex), "fork unlock");
}
Fork::~Fork() {
    Check(pthread_cond_destroy(&cond), "fork condition destroy");
    Check(pthread_mutex_destroy(&mutex), "fork mutex destroy");
}
