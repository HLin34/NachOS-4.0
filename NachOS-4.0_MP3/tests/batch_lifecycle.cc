#include "main.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

Kernel *kernel;
Debug *debug;
struct Halted {};
struct LauncherFinished {};
static int forks=0,halts=0;
static bool finishInsideFork=false,inFork=false;
static std::vector<Thread*> children;
static void Check(bool good,const char *reason) {
    if(!good) {fprintf(stderr,"FAIL: %s\n",reason);exit(1);}
}
static void Complete(Thread *thread) {
    delete thread->space;thread->space=NULL;
    kernel->ProgramFinished(thread);
}
extern "C" void __wrap__ZN6Thread4ForkEPFvPvES0_(Thread *thread,VoidFunctionPtr,void *arg) {
    Check(arg==thread,"launch argument matches registration");
    Check(kernel->getThread(thread->getID())==thread,"child registered before Fork");
    Check(thread->getID()==++forks,"monotonic reserved IDs");
    children.push_back(thread);inFork=true;
    if(finishInsideFork) Complete(thread);
    inFork=false;
}
extern "C" void __wrap__ZN6Thread6FinishEv(Thread *thread) {
    Check(thread==kernel->currentThread&&thread->getID()==0,"only launcher intercepted");
    throw LauncherFinished();
}
extern "C" void __wrap__ZN9Interrupt4HaltEv(Interrupt*) {
    Check(!inFork,"cannot halt in an early child before remaining launches");
    Check(kernel->numAvailPhysPage==NumPhysPages,"all frames free at completion");
    ++halts;throw Halted();
}
static void Initialize(const std::vector<char*> &args) {
    kernel=new Kernel(static_cast<int>(args.size()),const_cast<char**>(args.data()));
    kernel->stats=new Statistics();kernel->interrupt=new Interrupt();
    kernel->scheduler=new Scheduler();kernel->machine=new Machine(false);
    kernel->currentThread=new Thread(const_cast<char*>("main"),0);
    kernel->currentThread->setStatus(RUNNING);
}
int main(int argc,char **argv) {
    Check(argc==2,"case required");const char *name=argv[1];debug=new Debug(const_cast<char*>(""));
    std::vector<char*> args;args.push_back(const_cast<char*>("fixture"));
    int count=!strcmp(name,"capacity")?9:!strcmp(name,"empty")||!strcmp(name,"standalone")?0:2;
    for(int i=0;i<count;i++){args.push_back(const_cast<char*>("-e"));args.push_back(const_cast<char*>("unloaded"));}
    Initialize(args);
    Check(kernel->getThread(-1)==NULL&&kernel->getThread(10)==NULL,"bounded thread lookup");
    if(!strcmp(name,"standalone")) {
        Check(kernel->Exec(const_cast<char*>("unloaded"))==1,"standalone reservation");
        Complete(children[0]);Check(halts==0,"no batch shutdown before ExecAll");
        try {kernel->ExecAll();Check(false,"completed empty batch must halt");} catch(const Halted&) {}
        Check(halts==1,"one halt after explicit batch start");
    } else if(!strcmp(name,"early_launch")||!strcmp(name,"empty")) {
        finishInsideFork=true;
        try {kernel->ExecAll();Check(false,"all completed batch must halt");} catch(const Halted&) {}
        Check(forks==count&&halts==1,"launch guard closes only after every Fork");
    } else {
        try {kernel->ExecAll();Check(false,"main must finish while children remain");} catch(const LauncherFinished&) {}
        Check(halts==0&&forks==count,"live children survive launcher completion");
        if(!strcmp(name,"foreign")) {
            Thread foreign(const_cast<char*>("foreign"),1);
            kernel->ProgramFinished(&foreign);kernel->ProgramFinished(kernel->currentThread);
            Check(halts==0&&kernel->getThread(1)==children[0],"foreign/main notification cannot consume a child");
        }
        if(!strcmp(name,"two_live")) children.back()->setStatus(BLOCKED);
        for(int i=0;i<count-1;i++) {
            Complete(children[i]);Check(halts==0,"other live/blocked children prevent shutdown");
            Check(kernel->getThread(i+1)==NULL,"finished registration removed");
            if(!strcmp(name,"duplicate")) {kernel->ProgramFinished(children[i]);Check(halts==0,"duplicate completion ignored");}
        }
        children.back()->setStatus(RUNNING); // Model completion after an I/O wakeup.
        try {Complete(children.back());Check(false,"last child must halt");} catch(const Halted&) {}
        Check(halts==1,"one final halt");
    }
    puts("PASS");return 0;
}
