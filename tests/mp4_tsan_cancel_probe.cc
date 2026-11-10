// Isolate sanitizer cancellation cleanup handling using only one protected int.
#include <pthread.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static int value=0;
static void Cleanup(void*) {
    pthread_mutex_lock(&mutex); value++; pthread_mutex_unlock(&mutex);
}
static void *Canceled(void*) {
    pthread_cleanup_push(Cleanup,NULL);
    for(;;) usleep(1000);
    pthread_cleanup_pop(0);
    return NULL;
}
static void *Writer(void*) {
    for(int i=0;i<1000;i++) {
        pthread_mutex_lock(&mutex); value++; pthread_mutex_unlock(&mutex);
        usleep(100);
    }
    return NULL;
}
int main() {
    pthread_t canceled,writer;
    if(pthread_create(&canceled,NULL,Canceled,NULL) || pthread_create(&writer,NULL,Writer,NULL)) return 1;
    usleep(10000);
    if(pthread_cancel(canceled) || pthread_join(canceled,NULL) || pthread_join(writer,NULL)) return 1;
    pthread_mutex_lock(&mutex); bool good=value==1001; pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    puts(good?"PASS":"FAIL"); return good?0:1;
}
