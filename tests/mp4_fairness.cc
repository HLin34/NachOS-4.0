// Black-box scheduling tests: observe enqueuing through cond_wait, not private fields.
#include "table.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <unistd.h>

static std::atomic<int> waiting[16], entered[16], releaseMeal[16];
static std::atomic<int> eatingMask(0), violations(0), sequence(0);
static int order[256];
static thread_local int workerId = -1;
static int count = 5;
static bool spurious = false;
extern "C" int __real_pthread_cond_wait(pthread_cond_t*,pthread_mutex_t*);
extern "C" int __wrap_pthread_cond_wait(pthread_cond_t *cond,pthread_mutex_t *mutex) {
    if (workerId >= 0) {
        int attempt=waiting[workerId].fetch_add(1);
        if(spurious && attempt<3) return 0; // Mutex remains owned on a spurious return.
    }
    return __real_pthread_cond_wait(cond,mutex);
}
static void Check(bool good,const char *message) {
    if (!good) { fprintf(stderr,"FAIL: %s\n",message); exit(1); }
}
static void Await(std::atomic<int> &value,int minimum=1) {
    for (int i=0;i<3000;i++) { if(value.load()>=minimum) return; usleep(1000); }
    Check(false,"expected scheduling event timed out");
}
struct Worker { Table *table; int id; int rounds; bool controlled; };
struct Owned { Worker *worker; bool held; int mask; };
static void Release(void *arg) {
    Owned *owned=static_cast<Owned*>(arg);
    if(owned->held) {
        eatingMask.fetch_and(~owned->mask);
        owned->worker->table->signal(owned->worker->id);
    }
}
static void *Run(void *arg) {
    Worker *worker=static_cast<Worker*>(arg);
    workerId=worker->id;
    for(int round=0;round<worker->rounds;round++) {
        Owned owned={worker,false,(1<<worker->id)|(1<<((worker->id+1)%count))};
        pthread_cleanup_push(Release,&owned);
        worker->table->wait(worker->id);
        owned.held=true;
        if(eatingMask.fetch_or(owned.mask)&owned.mask) violations.fetch_add(1);
        if(worker->controlled) {
            int slot=sequence.fetch_add(1);
            Check(slot<256,"test grant-record capacity");
            order[slot]=worker->id;
        }
        entered[worker->id].fetch_add(1);
        if(worker->controlled) {
            while(!releaseMeal[worker->id].load()) usleep(100);
        } else usleep(worker->id==0?1:50); // One aggressive requester.
        pthread_cleanup_pop(1);
    }
    return NULL;
}
static pthread_t Start(Worker *worker) {
    pthread_t thread;
    Check(pthread_create(&thread,NULL,Run,worker)==0,"create");
    return thread;
}
static void Join(pthread_t thread,bool cancelled=false) {
    void *result=NULL;
    Check(pthread_join(thread,&result)==0,"join");
    if(cancelled) Check(result==PTHREAD_CANCELED,"cancelled thread result");
}
static void Cancel(pthread_t thread) { Check(pthread_cancel(thread)==0,"cancel"); Join(thread,true); }
static void Probe(Table &table) {
    Check(violations.load()==0 && eatingMask.load()==0,"no overlapping fork reservations or leaks");
    for(int i=0;i<count;i++) { table.wait(i); table.signal(i); }
}
static void Queue(pthread_t &thread,Worker &worker) {
    int before=waiting[worker.id].load();
    thread=Start(&worker); Await(waiting[worker.id],before+1);
}
int main(int argc,char **argv) {
    Check(argc>=2,"scenario required");
    const char *scenario=argv[1];
    if(argc==3) count=atoi(argv[2]);
    Check(count>=2 && count<=15,"participant range");
    Table table(count);
    Worker workers[16]; pthread_t threads[16];
    for(int i=0;i<count;i++) workers[i]=Worker{&table,i,1,true};
    spurious=strcmp(scenario,"spurious")==0;
    if(strcmp(scenario,"fifo")==0 || spurious) {
        Check(count==5,"FIFO layout requires five participants");
        table.wait(0);
        for(int i=1;i<count;i++) Queue(threads[i],workers[i]);
        // Releasing and immediately re-requesting cannot jump queued requests.
        table.signal(0); threads[0]=Start(&workers[0]);
        for(int i=1;i<=count;i++) {
            int id=i%count;
            Await(entered[id]);
            Check(sequence.load()==i && order[i-1]==id,"FIFO grant order with rapid re-entry");
            releaseMeal[id].store(1);
        }
        for(int i=0;i<count;i++) Join(threads[i]);
    } else if(strcmp(scenario,"parallel")==0) {
        Check(count==5,"parallel layout requires five participants");
        threads[0]=Start(&workers[0]); Await(entered[0]);
        threads[2]=Start(&workers[2]); Await(entered[2]);
        Check(eatingMask.load()==15,"disjoint meals overlap in time");
        releaseMeal[0].store(1); releaseMeal[2].store(1);
        Join(threads[0]);Join(threads[2]);
    } else if(strcmp(scenario,"cancel_head")==0) {
        Check(count==5,"head layout requires five participants");
        table.wait(0);
        Queue(threads[1],workers[1]); Queue(threads[2],workers[2]);
        Cancel(threads[1]);
        Await(entered[2]); // Disjoint successor must wake before holder 0 releases.
        Check(entered[1].load()==0,"cancelled head never granted");
        releaseMeal[2].store(1);Join(threads[2]);table.signal(0);
    } else if(strcmp(scenario,"cancel_middle")==0 || strcmp(scenario,"cancel_tail")==0 || strcmp(scenario,"cancel_all")==0) {
        Check(count==5,"cancellation layout requires five participants");
        table.wait(0);
        for(int i=1;i<count;i++) Queue(threads[i],workers[i]);
        if(strcmp(scenario,"cancel_all")==0) {
            for(int i=1;i<count;i++) Cancel(threads[i]);
        } else {
            int victim=strcmp(scenario,"cancel_middle")==0?2:4;
            Cancel(threads[victim]);
            if(victim==4) Queue(threads[4],workers[4]); // Tail removed then reused.
            for(int i=1;i<count;i++) releaseMeal[i].store(1);
            table.signal(0);
            for(int i=1;i<count;i++) if(i!=victim || victim==4) Join(threads[i]);
            Probe(table);puts("PASS");return 0;
        }
        table.signal(0);
    } else if(strcmp(scenario,"cancel_grant")==0) {
        for(int i=0;i<200;i++) {
            entered[1].store(0);releaseMeal[1].store(0);
            table.wait(0); Queue(threads[1],workers[1]);
            // Exercise cancellation both before and after reservation handoff.
            if(i%2==0) { Check(pthread_cancel(threads[1])==0,"request before release");table.signal(0); }
            else { table.signal(0);Await(entered[1]);Check(pthread_cancel(threads[1])==0,"request after grant"); }
            Join(threads[1],true); Probe(table);
        }
    } else if(strcmp(scenario,"progress")==0) {
        for(int i=0;i<count;i++) { workers[i].controlled=false;workers[i].rounds=300;threads[i]=Start(&workers[i]); }
        for(int i=0;i<count;i++) Join(threads[i]);
        for(int i=0;i<count;i++) Check(entered[i].load()==300,"every hungry participant completes all rounds");
    } else if(strcmp(scenario,"invalid_id")==0) {
        for(int id : {-1,count}) {
            bool rejected=false;
            try { table.wait(id); } catch(const std::out_of_range&) { rejected=true; }
            Check(rejected,"invalid wait id rejected"); rejected=false;
            try { table.signal(id); } catch(const std::out_of_range&) { rejected=true; }
            Check(rejected,"invalid release id rejected");
        }
    } else Check(false,"unknown scenario");
    Probe(table);puts("PASS");return 0;
}
