// Real loader capacity boundaries and failure cleanup; no replacement allocator.
#include "main.h"
#include "addrspace.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>

Kernel *kernel;
Debug *debug;
static void Check(bool condition,const char *why) {
    if(!condition) { fprintf(stderr,"FAIL: %s\n",why); exit(1); }
}
static int Descriptors() {
    DIR *directory=opendir("/proc/self/fd"); Check(directory!=NULL,"fd directory");
    int count=0; while(readdir(directory)!=NULL) count++;
    closedir(directory); return count;
}
static AddrSpace *Load(const char *path) {
    AddrSpace *space=new AddrSpace;
    Check(space->Load(const_cast<char*>(path)),"resident load");
    return space;
}
static void Reject(const char *path,int repeats) {
    bool bitmap[NumPhysPages];
    memcpy(bitmap,kernel->frameTable,sizeof bitmap);
    int free=kernel->NumFreeFrame, descriptors=Descriptors();
    char *memory=new char[MemorySize];
    memcpy(memory,kernel->machine->mainMemory,MemorySize);
    kernel->machine->WriteRegister(PCReg,40);
    kernel->machine->WriteRegister(NextPCReg,44);
    for(int i=0;i<repeats;i++) {
        AddrSpace *space=new AddrSpace;
        Check(!space->Load(const_cast<char*>(path)),"insufficient memory returns false");
        delete space;
        Check(kernel->NumFreeFrame==free,"failed load preserves free count");
        Check(memcmp(bitmap,kernel->frameTable,sizeof bitmap)==0,"failed load preserves bitmap");
        Check(memcmp(memory,kernel->machine->mainMemory,MemorySize)==0,"failed load preserves resident bytes");
        Check(Descriptors()==descriptors,"executable descriptor closed");
        Check(kernel->machine->ReadRegister(PCReg)==40 &&
              kernel->machine->ReadRegister(NextPCReg)==44,"loader exception does not advance user PC");
    }
    delete [] memory;
}
int main(int argc,char **argv) {
    Check(argc==4,"case and fixture paths required");
    char name[]="oom-test",flags[]=""; char *options[]={name};
    debug=new Debug(flags); kernel=new Kernel(1,options); kernel->Initialize();
    memset(kernel->machine->mainMemory,0xa5,MemorySize);
    const char *test=argv[1];
    if(strcmp(test,"oversized")==0 || strcmp(test,"image_exceeds_physical")==0) Reject(argv[2],3);
    else if(strcmp(test,"repeated_rejection")==0) Reject(argv[2],80);
    else if(strcmp(test,"remaining_capacity")==0 || strcmp(test,"zero_available")==0) {
        bool all=strcmp(test,"zero_available")==0;
        kernel->NumFreeFrame=0;
        for(int frame=0;frame<NumPhysPages;frame++) {
            kernel->frameTable[frame]=all || frame%2==0;
            if(!kernel->frameTable[frame]) kernel->NumFreeFrame++;
        }
        Reject(argv[2],3);
    } else if(strcmp(test,"allocation_rollback_guard")==0) {
        // Fault injection: a stale free counter makes the frame scan run out
        // after two allocations. The guard must undo only those allocations.
        for(int frame=0;frame<NumPhysPages;frame++) kernel->frameTable[frame]=frame>=2;
        kernel->NumFreeFrame=9;
        int descriptors=Descriptors();
        for(int cycle=0;cycle<3;cycle++) {
            AddrSpace *space=new AddrSpace;
            Check(!space->Load(const_cast<char*>(argv[2])),"partial allocation reports failure");
            delete space;
            Check(kernel->NumFreeFrame==9,"partial allocation restores prior accounting");
            for(int frame=0;frame<NumPhysPages;frame++) {
                Check(kernel->frameTable[frame]==(frame>=2),"partial bitmap rolled back");
                if(frame>=2) for(int byte=0;byte<PageSize;byte++)
                    Check((unsigned char)kernel->machine->mainMemory[frame*PageSize+byte]==0xa5,
                          "reserved frame unchanged");
            }
            Check(Descriptors()==descriptors,"partial allocation closes executable");
        }
    } else if(strcmp(test,"exact_fit_reuse")==0) {
        AddrSpace *resident=Load(argv[2]);
        Check(kernel->NumFreeFrame==0,"exact fit consumes all frames");
        Reject(argv[3],3);
        AddrSpace *retry=new AddrSpace;
        Check(!retry->Load(const_cast<char*>(argv[3])),"retry object first reports shortage");
        delete resident;
        Check(kernel->NumFreeFrame==NumPhysPages,"exact-fit release");
        Check(retry->Load(const_cast<char*>(argv[3])),"same failed object can load after release");
        delete retry;
        Check(kernel->NumFreeFrame==NumPhysPages,"memory reused after release");
    } else if(strcmp(test,"two_residents_reuse")==0) {
        AddrSpace *first=Load(argv[2]), *second=Load(argv[2]);
        Check(kernel->NumFreeFrame==8,"two residents leave eight frames");
        Reject(argv[3],3);
        delete first;
        AddrSpace *retry=Load(argv[3]);
        Check(kernel->NumFreeFrame==59,"retry shares memory with surviving resident");
        delete retry; delete second;
        Check(kernel->NumFreeFrame==NumPhysPages,"all residents reclaimed");
    } else Check(false,"unknown case");
    puts("PASS");
    return 0;
}
