// Check actual ownership after cond-wait cancellation using an ERRORCHECK mutex.
#include <pthread.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
static pthread_mutex_t mutex;
static pthread_cond_t cond=PTHREAD_COND_INITIALIZER;
static bool entered=false;
static void Check(int status) { if(status!=0) abort(); }
static void Unlock(void*) { Check(pthread_mutex_unlock(&mutex)); }
static void *Wait(void*) {
    Check(pthread_mutex_lock(&mutex));
    pthread_cleanup_push(Unlock,NULL);
    entered=true;
    for(;;) Check(pthread_cond_wait(&cond,&mutex));
    pthread_cleanup_pop(1);
    return NULL;
}
int main() {
    pthread_mutexattr_t attr;
    Check(pthread_mutexattr_init(&attr));
    Check(pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_ERRORCHECK));
    Check(pthread_mutex_init(&mutex,&attr));
    Check(pthread_mutexattr_destroy(&attr));
    pthread_t worker;
    Check(pthread_create(&worker,NULL,Wait,NULL));
    for(;;) {
        Check(pthread_mutex_lock(&mutex)); bool ready=entered; Check(pthread_mutex_unlock(&mutex));
        if(ready) break;
        usleep(100);
    }
    Check(pthread_cancel(worker)); Check(pthread_join(worker,NULL));
    Check(pthread_mutex_trylock(&mutex)); Check(pthread_mutex_unlock(&mutex));
    Check(pthread_cond_destroy(&cond)); Check(pthread_mutex_destroy(&mutex));
    puts("PASS: actual mutex ownership and post-cancel acquisition");
    return 0;
}
