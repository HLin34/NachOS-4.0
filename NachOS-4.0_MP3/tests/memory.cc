#include "main.h"
#include "addrspace.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
Kernel *kernel;Debug *debug;
static int failRead=0,reads=0;
extern "C" ssize_t __real_read(int,void*,size_t);
extern "C" ssize_t __wrap_read(int fd,void *buffer,size_t size) {
    if(fd>2&&failRead&&++reads==failRead) {failRead=0;return __real_read(fd,buffer,size?size-1:0);}
    return __real_read(fd,buffer,size);
}
static void Check(bool good,const char *why){if(!good){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
static char *Path(const char *base,const char *name){static char paths[8][2048];static int slot=0;slot=(slot+1)%8;snprintf(paths[slot],sizeof paths[slot],"%s/%s",base,name);return paths[slot];}
static int Descriptors(){DIR *d=opendir("/proc/self/fd");Check(d!=NULL,"fd directory");int n=0;while(readdir(d))++n;closedir(d);return n;}
static void Init(bool fragmented){
    debug=new Debug(const_cast<char*>(""));kernel=new Kernel(0,NULL);
    kernel->stats=new Statistics;kernel->interrupt=new Interrupt;kernel->scheduler=new Scheduler;
    kernel->machine=new Machine(false);kernel->fileSystem=new FileSystem;
    kernel->currentThread=new Thread(const_cast<char*>("fixture"),0);kernel->currentThread->setStatus(RUNNING);
    kernel->numAvailPhysPage=0;
    for(int i=0;i<NumPhysPages;++i){kernel->usedPhysPage[i]=fragmented&&i%2==0;if(!kernel->usedPhysPage[i])++kernel->numAvailPhysPage;}
    memset(kernel->machine->mainMemory,0xa5,MemorySize);
}
static void Reject(char *path,int repeats){
    bool before[NumPhysPages];memcpy(before,kernel->usedPhysPage,sizeof before);
    int free=kernel->numAvailPhysPage,fd=Descriptors();char memory[MemorySize];memcpy(memory,kernel->machine->mainMemory,MemorySize);
    TranslationEntry *mapping=kernel->machine->pageTable;unsigned pages=mapping?kernel->machine->pageTableSize:0;
    kernel->machine->WriteRegister(PCReg,40);kernel->machine->WriteRegister(NextPCReg,44);
    for(int n=0;n<repeats;++n){
        AddrSpace *space=new AddrSpace;Check(!space->Load(path),"load must reject safely");delete space;
        Check(free==kernel->numAvailPhysPage&&memcmp(before,kernel->usedPhysPage,sizeof before)==0,"failure preserves frame ownership");
        for(int p=0;p<NumPhysPages;++p)if(before[p])Check(memcmp(memory+p*PageSize,kernel->machine->mainMemory+p*PageSize,PageSize)==0,"resident/reserved bytes preserved");
        Check(kernel->machine->pageTable==mapping&&(!mapping||kernel->machine->pageTableSize==pages),"other active mapping preserved");
        Check(kernel->machine->ReadRegister(PCReg)==40&&kernel->machine->ReadRegister(NextPCReg)==44,"load rejection does not trap/advance user PC");
        Check(Descriptors()==fd,"executable descriptor closed");
    }
}
static void Layout(char *path){
    char expected[2048];snprintf(expected,sizeof expected,"%s.expected",path);FILE *file=fopen(expected,"rb");Check(file!=NULL,"expected file");
    unsigned pages;Check(fread(&pages,4,1,file)==1,"expected page count");std::vector<unsigned char> bytes(pages*PageSize),ro(pages);
    Check(fread(bytes.data(),1,bytes.size(),file)==bytes.size()&&fread(ro.data(),1,pages,file)==pages,"expected image");fclose(file);
    bool before[NumPhysPages];memcpy(before,kernel->usedPhysPage,sizeof before);int free=kernel->numAvailPhysPage;
    for(int cycle=0;cycle<3;cycle++){
        AddrSpace *space=new AddrSpace;Check(space->Load(path),"valid layout loads");space->RestoreState();
        Check(kernel->machine->pageTableSize==pages&&kernel->numAvailPhysPage==free-static_cast<int>(pages),"page extent/allocation count");
        bool seen[NumPhysPages]={false};
        for(unsigned p=0;p<pages;p++){
            TranslationEntry &e=kernel->machine->pageTable[p];Check(e.valid&&!e.dirty&&!e.use&&e.readOnly==(ro[p]!=0),"fresh flags and read-only sharing policy");
            Check(e.physicalPage>=0&&e.physicalPage<NumPhysPages&&!before[e.physicalPage]&&!seen[e.physicalPage],"unique unreserved physical frames");seen[e.physicalPage]=true;
        }
        for(unsigned v=0;v<bytes.size();v++){unsigned physical;Check(space->Translate(v,&physical,0)==NoException,"read translation");Check(static_cast<unsigned char>(kernel->machine->mainMemory[physical])==bytes[v],"payload/gaps/BSS/stack and exact guards");}
        for(unsigned p=0;p<pages;p++)if(ro[p]){unsigned physical;Check(space->Translate(p*PageSize,&physical,1)==ReadOnlyException,"read-only write rejected");}
        for(int p=0;p<NumPhysPages;p++)if(before[p])for(int n=0;n<PageSize;n++)Check(static_cast<unsigned char>(kernel->machine->mainMemory[p*PageSize+n])==0xa5,"reserved frame untouched");
        delete space;Check(kernel->machine->pageTable==NULL,"destroyed active mapping detached");
        Check(kernel->numAvailPhysPage==free&&memcmp(before,kernel->usedPhysPage,sizeof before)==0,"complete reclaim without double free");
        for(int p=0;p<NumPhysPages;p++)if(!before[p])memset(kernel->machine->mainMemory+p*PageSize,0xa5,PageSize);
    }
}
int main(int argc,char **argv){
    Check(argc==3,"case and fixtures required");const char *test=argv[1],*base=argv[2];
    bool fragmented=strcmp(test,"full_capacity")!=0&&strcmp(test,"resident_isolation")!=0;Init(fragmented);
    if(strncmp(test,"layout_",7)==0)Layout(Path(base,test+7));
    else if(strncmp(test,"reject_",7)==0)Reject(Path(base,test+7),3);
    else if(!strcmp(test,"valid_control")){AddrSpace *s=new AddrSpace;Check(s->Load(Path(base,"small")),"basic valid load");delete s;}
    else if(!strcmp(test,"full_capacity"))Layout(Path(base,"full"));
    else if(!strcmp(test,"zero_available")){for(int p=0;p<NumPhysPages;p++)kernel->usedPhysPage[p]=true;kernel->numAvailPhysPage=0;Reject(Path(base,"small"),3);}
    else if(!strcmp(test,"inconsistent_counter")){for(int p=0;p<NumPhysPages;p++)kernel->usedPhysPage[p]=p>=2;kernel->numAvailPhysPage=12;Reject(Path(base,"small"),3);}
    else if(!strcmp(test,"partial_short_read")){reads=0;failRead=3;Reject(Path(base,"unaligned"),1);Check(failRead==0,"short read reached after allocation");}
    else if(!strcmp(test,"repeated_failure"))Reject(Path(base,"oversized"),80);
    else if(!strcmp(test,"resident_isolation")){
        AddrSpace *resident=new AddrSpace;Check(resident->Load(Path(base,"small")),"resident load");resident->RestoreState();
        Reject(Path(base,"full"),3);reads=0;failRead=3;Reject(Path(base,"unaligned"),1);delete resident;Check(kernel->numAvailPhysPage==NumPhysPages,"resident reclaim");
    } else if(!strcmp(test,"retry_same_object")){
        AddrSpace *s=new AddrSpace;int free=kernel->numAvailPhysPage;
        Check(!s->Load(Path(base,"truncated")),"initial failure");
        reads=0;failRead=3;Check(!s->Load(Path(base,"unaligned")),"allocated short-read rollback");
        Check(failRead==0&&kernel->numAvailPhysPage==free,"rollback resets same object's ownership");
        Check(s->Load(Path(base,"small")),"retry empty object after rollback");delete s;
        Check(kernel->numAvailPhysPage==free,"retried object reclaims once");
    } else if(!strcmp(test,"already_loaded")){
        AddrSpace *s=new AddrSpace;Check(s->Load(Path(base,"small")),"first load");int free=kernel->numAvailPhysPage;
        Check(!s->Load(Path(base,"small"))&&kernel->numAvailPhysPage==free,"second load rejected without leaks");delete s;
    } else if(!strcmp(test,"invalid_translation")){
        AddrSpace *s=new AddrSpace;Check(s->Load(Path(base,"small")),"translation fixture");s->RestoreState();TranslationEntry saved=kernel->machine->pageTable[0];unsigned physical;
        kernel->machine->pageTable[0].valid=false;Check(s->Translate(0,&physical,0)==PageFaultException,"invalid page rejected");kernel->machine->pageTable[0]=saved;
        kernel->machine->pageTable[0].physicalPage=-1;Check(s->Translate(0,&physical,0)==BusErrorException,"negative physical frame rejected");kernel->machine->pageTable[0]=saved;
        kernel->machine->pageTable[0].physicalPage=NumPhysPages;Check(s->Translate(0,&physical,0)==BusErrorException,"upper physical bound rejected");kernel->machine->pageTable[0]=saved;delete s;
    } else Check(false,"unknown case");
    puts("PASS");return 0;
}
