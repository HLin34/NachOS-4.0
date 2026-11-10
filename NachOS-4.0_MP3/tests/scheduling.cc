#include "main.h"
#include "alarm.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

Kernel *kernel;
Debug *debug;
static int dispatches=0,requests=0;
extern "C" void __wrap_SWITCH(Thread*,Thread*) { ++dispatches; }
extern "C" void __wrap__ZN9Interrupt13YieldOnReturnEv(Interrupt*) { ++requests; }
static void Check(bool condition,const char *message) {
    if(!condition) { fprintf(stderr,"FAIL: %s\n",message); exit(1); }
}
static bool Equal(double a,double b) { return fabs(a-b)<1e-9; }
static Thread *New(int id,double estimate=0,double elapsed=0) {
    Thread *t=new Thread(const_cast<char*>("fixture"),id);
    t->set_approximate_burst_time(estimate);t->set_execution_time(elapsed);
    return t;
}
static void Clock(int delta) { kernel->stats->totalTicks=1000+delta; }
static void AlarmTick() { static_cast<CallBackObj*>(kernel->alarm)->CallBack(); }
static void Ready(Thread *t) { kernel->scheduler->ReadyToRun(t); }
int main(int argc,char **argv) {
    Check(argc==2,"case argument");const char *name=argv[1];
    debug=new Debug(const_cast<char*>("z"));
    // Minimal real kernel services; do not exercise the separately audited
    // uninitialized ExecAll counters or perform any physical context switch.
    kernel=new Kernel(0,NULL);
    kernel->stats=new Statistics();Clock(0);
    kernel->interrupt=new Interrupt();kernel->scheduler=new Scheduler();
    kernel->alarm=new Alarm(false);kernel->machine=NULL;
    Thread *current=New(0);kernel->currentThread=current;
    current->setStatus(RUNNING);current->set_start_time(1000);
    if(!strcmp(name,"initial")) {
        Check(Equal(current->get_approximate_burst_time(),0)&&Equal(current->get_execution_time(),0),"initial burst state");
    } else if(!strcmp(name,"first_burst")) {
        Ready(New(1));kernel->stats->userTicks=40;kernel->stats->systemTicks=30;Clock(70);
        current->Sleep(false);
        Check(Equal(current->get_approximate_burst_time(),35)&&Equal(current->get_execution_time(),0),"first user+kernel burst trains at block");
    } else if(!strcmp(name,"last_slice")) {
        current->set_approximate_burst_time(10);current->set_execution_time(30);Clock(50);Ready(New(1));
        current->Sleep(false);Check(Equal(current->get_approximate_burst_time(),45),"final slice included");
    } else if(!strcmp(name,"zero_burst")) {
        current->set_approximate_burst_time(8);Ready(New(1));current->Sleep(false);
        Check(Equal(current->get_approximate_burst_time(),4),"zero burst still updates recurrence");
    } else if(!strcmp(name,"kernel_and_no_double_count")) {
        kernel->stats->userTicks=20;kernel->stats->systemTicks=30;Clock(50);
        current->AccountRunningTime();current->AccountRunningTime();
        Check(Equal(current->get_execution_time(),50),"kernel time counted exactly once");
    } else if(!strcmp(name,"ready_time_excluded")) {
        Thread *other=New(1);Ready(other);Clock(20);current->Yield();
        Check(Equal(current->get_execution_time(),20),"first slice preserved on ready transition");
        Clock(100);other->Sleep(false); // Real dispatcher resumes current, new start=1100.
        Check(kernel->currentThread==current&&Equal(current->get_start_time(),1100),"resume resets clock");
        Clock(110);Ready(other);current->Sleep(false);
        Check(Equal(current->get_approximate_burst_time(),15),"80 ready ticks excluded; 20+10 CPU ticks included");
    } else if(!strcmp(name,"clamped_fifo")) {
        Thread *a=New(1,1,4),*b=New(2,2,3);Ready(a);Ready(b);
        Check(Equal(a->RemainingBurst(),0)&&Equal(b->RemainingBurst(),0),"negative remaining clamped");
        Check(kernel->scheduler->FindNextToRun()==a&&kernel->scheduler->FindNextToRun()==b,"zero tie keeps FIFO ready order");
    } else if(!strcmp(name,"live_remaining")) {
        current->set_approximate_burst_time(100);current->set_execution_time(10);Clock(30);
        Check(Equal(current->RemainingBurstAt(1030),60),"running slice in live comparison");
        Thread *queued=New(1,100,10);Ready(queued);
        Check(Equal(queued->RemainingBurstAt(9999),90),"ready clock is frozen");
    } else if(!strcmp(name,"shorter_timer")) {
        current->set_approximate_burst_time(100);Thread *next=New(1,20);Ready(next);Clock(10);
        AlarmTick();Check(requests==1&&dispatches==0,"timer only requests deferred switch");
        current->Yield();Check(dispatches==1&&kernel->currentThread==next,"shorter thread selected after callback");
        Check(Equal(current->get_execution_time(),10)&&Equal(current->get_approximate_burst_time(),100),"preemption preserves estimate and accumulates CPU time");
        Check(!kernel->scheduler->ConsumeTimerPreemption(),"request consumed once");
    } else if(!strcmp(name,"longer_timer")||!strcmp(name,"equal_timer")||!strcmp(name,"zero_timer")) {
        double estimate=!strcmp(name,"zero_timer")?0:20;
        current->set_approximate_burst_time(estimate);
        Thread *next=New(1,!strcmp(name,"longer_timer")?100:estimate);Ready(next);
        AlarmTick();Check(requests==0&&dispatches==0,"equal/longer candidate cannot preempt");
        Check(kernel->scheduler->FindNextToRun()==next,"comparison does not dequeue");
    } else if(!strcmp(name,"empty_timer")) {
        current->set_approximate_burst_time(100);AlarmTick();
        Check(requests==0&&dispatches==0&&!kernel->scheduler->ShouldPreempt(),"empty queue no request");
    } else if(!strcmp(name,"idle_timer")) {
        current->set_approximate_burst_time(100);Ready(New(1));kernel->interrupt->setStatus(IdleMode);
        AlarmTick();Check(requests==0&&dispatches==0,"idle callback cannot request a preemption");
    } else if(!strcmp(name,"timer_recheck")) {
        current->set_approximate_burst_time(100);Thread *next=New(1,50);Ready(next);AlarmTick();
        Check(requests==1,"initial timer request");Clock(60);current->Yield();
        Check(dispatches==0&&kernel->currentThread==current,"recheck prevents stale request from switching");
        Check(kernel->scheduler->FindNextToRun()==next,"stale request leaves queue intact");
    } else if(!strcmp(name,"wake_defers")) {
        current->set_approximate_burst_time(100);Ready(New(1));
        Check(requests==0&&dispatches==0&&!kernel->scheduler->ConsumeTimerPreemption(),"ready transition does not preempt");
        AlarmTick();Check(requests==1&&dispatches==0,"only callback requests dispatch");
    } else if(!strcmp(name,"voluntary_yield")) {
        Thread *next=New(1);Ready(next);current->Yield();
        Check(requests==0&&dispatches==1&&kernel->currentThread==next,"voluntary yield still hands off an equal candidate");
    } else if(!strcmp(name,"settle_before_insert")) {
        current->set_approximate_burst_time(100);Thread *first=New(1,10),*second=New(2,30);
        Ready(first);Ready(second);Clock(80);current->Yield();
        Check(kernel->currentThread==first&&kernel->scheduler->FindNextToRun()==current,"updated remaining=20 inserted before remaining=30");
    } else if(!strcmp(name,"multiple_preemptions")) {
        current->set_approximate_burst_time(100);Thread *other=New(1,20);Ready(other);Clock(10);
        AlarmTick();current->Yield();Check(Equal(current->get_execution_time(),10),"first preemption accumulates");
        Clock(40);other->Sleep(false);Clock(50);Ready(other);AlarmTick();current->Yield();
        Check(Equal(current->get_execution_time(),20),"second preemption accumulates across resume");
        Clock(80);other->Sleep(false);Clock(90);Ready(other);current->Sleep(false);
        Check(Equal(current->get_approximate_burst_time(),65)&&requests==2,"three slices total 30; two ready intervals excluded");
    } else if(!strcmp(name,"negative_remaining_timer")) {
        current->set_approximate_burst_time(1);current->set_execution_time(4);Ready(New(1,2,3));
        AlarmTick();Check(requests==0&&!kernel->scheduler->ShouldPreempt(),"two exhausted estimates tie at zero");
    } else if(!strcmp(name,"fractional_estimate")) {
        current->set_approximate_burst_time(1.5);current->set_execution_time(0.5);Ready(New(1,0.75));
        Check(kernel->scheduler->ShouldPreempt(),"double remaining comparison without truncation");
    } else Check(false,"unknown case");
    puts("PASS");return 0;
}
