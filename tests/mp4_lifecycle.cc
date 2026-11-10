// Test-only interposition: phase control, creation failure, and destruction checks.
#include "philosopher.hpp"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

static std::atomic<int> waits(0), thinkers(0), eaters(0), active(0), creates(0), successfulCreates(0), joins(0), badDestroy(0), destroys(0);
static const char *scenario="application";
extern "C" {
unsigned __real_sleep(unsigned);
int __real_pthread_cond_wait(pthread_cond_t*,pthread_mutex_t*);
int __real_pthread_create(pthread_t*,const pthread_attr_t*,void*(*)(void*),void*);
int __real_pthread_join(pthread_t,void**);
int __real_pthread_mutex_destroy(pthread_mutex_t*);
int __real_pthread_mutex_init(pthread_mutex_t*,const pthread_mutexattr_t*);
int __real_pthread_cond_destroy(pthread_cond_t*);
struct Launch { void *(*fn)(void*); void *arg; };
int __wrap_pthread_mutex_init(pthread_mutex_t *mutex,const pthread_mutexattr_t *attr) {
    if(attr!=NULL) return __real_pthread_mutex_init(mutex,attr);
    pthread_mutexattr_t checked;
    if(pthread_mutexattr_init(&checked)!=0) abort();
    if(pthread_mutexattr_settype(&checked,PTHREAD_MUTEX_ERRORCHECK)!=0) abort();
    int status=__real_pthread_mutex_init(mutex,&checked);
    pthread_mutexattr_destroy(&checked);
    return status;
}
static void Finished(void*) { active.fetch_sub(1); }
static void *LaunchThread(void *arg) {
    Launch launch=*static_cast<Launch*>(arg); delete static_cast<Launch*>(arg);
    active.fetch_add(1);
    void *result;
    pthread_cleanup_push(Finished,NULL);
    result=launch.fn(launch.arg);
    pthread_cleanup_pop(1);
    return result;
}
int __wrap_pthread_create(pthread_t *thread,const pthread_attr_t *attr,void *(*fn)(void*),void *arg) {
    int call=creates.fetch_add(1)+1;
    const char *failure=getenv("MP4_FAIL_CREATE");
    if(failure && call==atoi(failure)) return EAGAIN;
    Launch *launch=new Launch{fn,arg};
    int result=__real_pthread_create(thread,attr,LaunchThread,launch);
    if(result!=0) delete launch;
    else successfulCreates.fetch_add(1);
    return result;
}
int __wrap_pthread_join(pthread_t thread,void **result) {
    int status=__real_pthread_join(thread,result);
    if(status==0) joins.fetch_add(1);
    return status;
}
int __wrap_pthread_cond_wait(pthread_cond_t *cond,pthread_mutex_t *mutex) {
    waits.fetch_add(1);
    return __real_pthread_cond_wait(cond,mutex);
}
unsigned __wrap_sleep(unsigned seconds) {
    if(strcmp(scenario,"application")==0) { usleep(seconds==RUNTIME?20000:1000); return 0; }
    if(strcmp(scenario,"sanitizer_workload")==0) {
        static thread_local int steps=0;
        // Exit outside a libc sleep interceptor. Forced cancellation inside
        // those interceptors has separately reproduced checker limitations.
        if(++steps>30) pthread_exit(NULL);
        usleep(100);
        return 0;
    }
    if(seconds==1) {
        thinkers.fetch_add(1);
        usleep(strcmp(scenario,"cancel_thinking")==0?10000000:100);
    } else {
        eaters.fetch_add(1);
        usleep(strcmp(scenario,"cancel_eating")==0?10000000:200);
    }
    return 0;
}
int __wrap_pthread_mutex_destroy(pthread_mutex_t *mutex) {
    destroys.fetch_add(1);
    if(active.load()!=0) badDestroy.fetch_add(1);
    int status=__real_pthread_mutex_destroy(mutex);
    if(status!=0) badDestroy.fetch_add(1);
    return status;
}
int __wrap_pthread_cond_destroy(pthread_cond_t *cond) {
    destroys.fetch_add(1);
    if(active.load()!=0) badDestroy.fetch_add(1);
    int status=__real_pthread_cond_destroy(cond);
    if(status!=0) badDestroy.fetch_add(1);
    return status;
}
}
#ifdef MP4_WRAPPERS_ONLY
__attribute__((destructor)) static void AuditApplication() {
    bool good=active.load()==0 && badDestroy.load()==0 && successfulCreates.load()==joins.load()
              && destroys.load()==2*(PHILOSOPHERS+1);
    fprintf(stderr,"[mp4-audit] active=%d joined=%d started=%d destroyed=%d bad=%d\n",
            active.load(),joins.load(),successfulCreates.load(),destroys.load(),badDestroy.load());
    if(!good) _Exit(90);
}
#endif
static void Check(bool value,const char *why) {
    if(!value) { fprintf(stderr,"FAIL: %s\n",why); exit(1); }
}
static void Await(std::atomic<int> &counter,int before) {
    for(int retry=0;retry<2000;retry++) { if(counter.load()>before) return; usleep(1000); }
    Check(false,"expected worker phase was not reached");
}
static std::atomic<bool> extraAdmission(false);
static void *ProbeTable(void *arg) {
    Table *table=static_cast<Table*>(arg);
    table->wait(1); extraAdmission.store(true); table->signal(1); return NULL;
}
static void Probe(Fork **forks,Table *table) {
    for(int i=0;i<PHILOSOPHERS;i++) { forks[i]->wait(); forks[i]->signal(); }
    table->wait(0);
    int before=waits.load(); pthread_t probe;
    Check(pthread_create(&probe,NULL,ProbeTable,table)==0,"capacity probe starts");
    Await(waits,before);
    Check(!extraAdmission.load(),"conflicting reservation must remain blocked during cancellation");
    Check(pthread_cancel(probe)==0 && pthread_join(probe,NULL)==0,"capacity probe cancels cleanly");
    table->signal(0);
}
static void *Turns(void *arg) {
    Table *table=static_cast<Table*>(arg);
    for(int i=0;i<1000;i++) { table->updateCurrentTurn(); table->getCurrentTurn(); }
    return NULL;
}
#ifndef MP4_WRAPPERS_ONLY
int main(int argc,char **argv) {
    Check(argc==2,"scenario required"); scenario=argv[1];
    if(strcmp(scenario,"invalid_table")==0) {
        bool rejected=false;
        try { Table invalid(1); } catch(const std::exception&) { rejected=true; }
        Check(rejected,"one-person table rejected"); puts("PASS"); return 0;
    }
    Fork *forks[PHILOSOPHERS];
    for(int i=0;i<PHILOSOPHERS;i++) forks[i]=new Fork;
    Table *table=new Table(PHILOSOPHERS);
    if(strcmp(scenario,"turn_race")==0) {
        pthread_t threads[4];
        for(int i=0;i<4;i++) Check(pthread_create(&threads[i],NULL,Turns,table)==0,"turn worker starts");
        for(int i=0;i<4;i++) Check(pthread_join(threads[i],NULL)==0,"turn worker joins");
        Check(table->getCurrentTurn()==4000%PHILOSOPHERS,"all turn increments preserved");
    } else {
        bool workload=strcmp(scenario,"sanitizer_workload")==0;
        bool stress=strcmp(scenario,"stress")==0 || workload;
        int count=stress?PHILOSOPHERS:1;
        Philosopher *workers[PHILOSOPHERS];
        if(strcmp(scenario,"cancel_table")==0)
            table->wait(1);
        if(strcmp(scenario,"cancel_left")==0) forks[0]->wait();
        if(strcmp(scenario,"cancel_right")==0) forks[1]->wait();
        int before=waits.load();
        for(int i=0;i<count;i++) workers[i]=new Philosopher(i,forks[i],forks[(i+1)%PHILOSOPHERS],table);
        for(int i=0;i<count;i++) workers[i]->start();
        if(!workload) {
            if(strcmp(scenario,"cancel_thinking")==0) Await(thinkers,0);
            else if(strcmp(scenario,"cancel_eating")==0) Await(eaters,0);
            else if(!stress) Await(waits,before);
            else usleep(30000);
            for(int i=count-1;i>=0;i--) Check(workers[i]->cancel()==0,"cancel request succeeds");
        }
        for(int i=0;i<count;i++) Check(workers[i]->join()==0,"join succeeds");
        Check(active.load()==0,"all workers terminated");
        if(strcmp(scenario,"cancel_table")==0)
            table->signal(1);
        if(strcmp(scenario,"cancel_left")==0) forks[0]->signal();
        if(strcmp(scenario,"cancel_right")==0) forks[1]->signal();
        for(int i=0;i<count;i++) delete workers[i];
    }
    Probe(forks,table);
    for(int i=0;i<PHILOSOPHERS;i++) delete forks[i];
    delete table;
    Check(active.load()==0 && badDestroy.load()==0,"resources destroyed only after joins");
    puts("PASS"); return 0;
}
#endif
