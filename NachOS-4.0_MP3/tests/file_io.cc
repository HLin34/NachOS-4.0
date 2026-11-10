// Real file/syscall objects with fault injection confined to host file calls.
#include <iostream>
#include <new>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
enum { HostEINTR=EINTR, HostEIO=EIO, HostEMFILE=EMFILE, HostENOSPC=ENOSPC, HostESPIPE=ESPIPE };
#include "main.h"
#include "syscall.h"
Kernel *kernel; Debug *debug;
static const char *fault="";
static int calls=0,openCalls=0,closeCalls=0;
static bool watch=false;
struct Finished {};
static void Check(bool ok,const char *why){if(!ok){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
extern "C" int __real_open(const char*,int,...);
extern "C" ssize_t __real_read(int,void*,size_t);
extern "C" ssize_t __real_write(int,const void*,size_t);
extern "C" off_t __real_lseek(int,off_t,int);
extern "C" int __real_close(int);
extern "C" int __real_fstat(int,struct stat*);
extern "C" int __wrap_fstat(int fd,struct stat*info){
 if(watch&&!strcmp(fault,"length_error")){errno=HostEIO;return -1;}
 if(watch&&!strcmp(fault,"length_eintr")&&calls++==0){errno=HostEINTR;return -1;}
 return __real_fstat(fd,info);
}
extern "C" void *__real__Znwj(unsigned int);
extern "C" void *__wrap__Znwj(unsigned int size){
 if(watch&&!strcmp(fault,"allocation_error")&&size==sizeof(OpenFile))throw std::bad_alloc();
 return __real__Znwj(size);
}
extern "C" int __wrap_open(const char*p,int flags,...){
 mode_t mode=0;if(flags&O_CREAT){va_list a;va_start(a,flags);mode=va_arg(a,int);va_end(a);}
 if(watch){++openCalls;if(!strcmp(fault,"open_error")){errno=HostEMFILE;return -1;}if(!strcmp(fault,"open_eintr")&&calls++==0){errno=HostEINTR;return -1;}}
 return __real_open(p,flags,mode);
}
extern "C" ssize_t __wrap_read(int fd,void*b,size_t n){
 if(watch&&fd>2){if(!strcmp(fault,"read_error")){errno=HostEIO;return -1;}if(!strcmp(fault,"read_eintr")&&calls++==0){errno=HostEINTR;return -1;}if(!strcmp(fault,"short_read")&&n>2)n=2;}
 return __real_read(fd,b,n);
}
extern "C" ssize_t __wrap_write(int fd,const void*b,size_t n){
 if(watch&&fd>2){if(!strcmp(fault,"write_error")){errno=HostENOSPC;return -1;}if(!strcmp(fault,"write_eintr")&&calls++==0){errno=HostEINTR;return -1;}if(!strcmp(fault,"short_write")&&n>2)n=2;if(!strcmp(fault,"zero_write"))return 0;}
 return __real_write(fd,b,n);
}
extern "C" off_t __wrap_lseek(int fd,off_t p,int whence){
 if(watch&&fd>2){if(!strcmp(fault,"seek_error")){errno=HostESPIPE;return -1;}if(!strcmp(fault,"seek_eintr")&&calls++==0){errno=HostEINTR;return -1;}}
 return __real_lseek(fd,p,whence);
}
extern "C" int __wrap_close(int fd){
 if(watch&&fd>2){++closeCalls;int result=__real_close(fd);if(!strcmp(fault,"close_error")||!strcmp(fault,"close_eintr")){errno=!strcmp(fault,"close_eintr")?HostEINTR:HostEIO;return -1;}return result;}
 return __real_close(fd);
}
extern "C" void __wrap__ZN6Kernel15ProgramFinishedEP6Thread(Kernel*,Thread *t){
 Check(t==kernel->currentThread,"finished current owner");
 for(int i=0;i<20;i++)Check(kernel->fileSystem->OpenFileTable[i]==NULL,"finish closes files before batch notification");
 throw Finished();
}
static int Fds(){DIR*d=opendir("/proc/self/fd");Check(d!=NULL,"fd inventory");int n=0;while(readdir(d))++n;closedir(d);return n;}
static void Fault(const char*s){fault=s;calls=0;watch=true;}
static void Clear(){watch=false;fault="";}
static int Trap(int type,int a=0,int b=0,int c=0){Machine*m=kernel->machine;m->WriteRegister(2,type);m->WriteRegister(4,a);m->WriteRegister(5,b);m->WriteRegister(6,c);m->WriteRegister(PCReg,40);m->WriteRegister(NextPCReg,44);ExceptionHandler(SyscallException);Check(m->ReadRegister(PCReg)==44&&m->ReadRegister(NextPCReg)==48,"error and success advance PC once");return m->ReadRegister(2);}
static int Open(Thread*t){return kernel->fileSystem->OpenAFile(const_cast<char*>("data.bin"),t);}
int main(int argc,char**argv){
 Check(argc==2,"case required");const char*name=argv[1];
 FILE*f=fopen("data.bin","wb");Check(f!=NULL,"fixture");fwrite("abcdef",1,6,f);fclose(f);
 debug=new Debug(const_cast<char*>(""));kernel=new Kernel(0,NULL);kernel->stats=new Statistics;kernel->interrupt=new Interrupt;kernel->scheduler=new Scheduler;kernel->machine=new Machine(false);kernel->fileSystem=new FileSystem;
 Thread*owner=new Thread(const_cast<char*>("owner"),1);owner->setStatus(RUNNING);kernel->currentThread=owner;Thread*other=new Thread(const_cast<char*>("other"),2);
 // A real loaded address space supports end-to-end trap copy-in/out.
 FILE*image=fopen("fixture.noff","wb");int header[13]={0xbadfad,0,52,32,0,0,0,0,0,0,128,0,1024};fwrite(header,4,13,image);char zero[32]={0};fwrite(zero,1,32,image);fclose(image);
 AddrSpace*space=new AddrSpace;Check(space->Load(const_cast<char*>("fixture.noff")),"load syscall fixture");owner->space=space;space->RestoreState();
 char*mem=kernel->machine->mainMemory; // This freshly empty bitmap maps virtual page 0 to frame 0.
 memcpy(mem+64,"data.bin",9);memcpy(mem+80,"WXYZ",4);memset(mem+96,'!',8);
 int baseline=Fds();FileSystem*fs=kernel->fileSystem;
 if(!strcmp(name,"allocation_error")){
  Fault("allocation_error");Check(Trap(SC_Open,64)==-1,"allocation failure returned");Clear();Check(Fds()==baseline,"failed allocation closes already-open fd");Check(Trap(SC_Open,64)==1,"failed allocation leaves slot free");
 }else if(!strcmp(name,"create_error")||!strcmp(name,"open_error")){
  Fault("open_error");Check(Trap(!strcmp(name,"create_error")?SC_Create:SC_Open,64)==(!strcmp(name,"create_error")?0:-1),"host open error returned");Clear();Check(Fds()==baseline,"failed open leaks no descriptor");Check(Trap(SC_Open,64)>0,"recover after open error");
 }else if(!strcmp(name,"missing_parent")){
  memcpy(mem+64,"missing/child",14);Check(Trap(SC_Create,64)==0,"missing directory is recoverable");
 }else if(!strcmp(name,"missing_file")){
  memcpy(mem+64,"absent.bin",11);Check(Trap(SC_Open,64)==-1,"missing file is recoverable");
 }else if(!strcmp(name,"open_eintr")||!strcmp(name,"create_eintr")){
  Fault("open_eintr");Check(Trap(!strcmp(name,"create_eintr")?SC_Create:SC_Open,64)>0,"retry interrupted open");Check(openCalls==2,"one retry");Clear();
 }else if(!strcmp(name,"create_close_error")){
  Fault("close_error");Check(Trap(SC_Create,64)==0,"create reports close failure");Check(closeCalls==1,"create closes once");Clear();Check(Fds()==baseline,"failed create close releases fd");
 }else if(!strcmp(name,"capacity")){
  for(int i=1;i<=20;i++){Check(Open(owner)==i,"IDs remain 1..20");}
  int full=Fds();Fault("");for(int i=0;i<100;i++)Check(Open(owner)==-1,"full table rejected");Check(openCalls==0,"full table does not open host files");Clear();Check(Fds()==full,"repeated full opens do not leak");Check(fs->CloseFile(7,owner)==1&&Open(owner)==7,"reuse released slot");fs->CloseAllFor(owner);Check(Fds()==baseline,"all owned fds reclaimed");
 }else if(!strcmp(name,"cross_owner")){
  int id=Open(owner);kernel->currentThread=other;Check(Trap(SC_Read,96,4,id)==-1&&Trap(SC_Write,80,4,id)==-1&&Trap(SC_Close,id)==-1,"foreign syscall handle rejected");kernel->currentThread=owner;Check(Trap(SC_Read,96,4,id)==4&&!memcmp(mem+96,"abcd",4),"foreign operations leave owner offset unchanged");
 }else if(!strcmp(name,"owner_cleanup")||!strcmp(name,"owner_cleanup_error")){
  int a=Open(owner),b=Open(other);if(!strcmp(name,"owner_cleanup_error"))Fault("close_error");fs->CloseAllFor(owner);fs->CloseAllFor(owner);Clear();Check(fs->CloseFile(a,owner)==-1,"owner cleanup idempotent");char data[2];Check(fs->ReadFile(data,2,b,other)==2&&!memcmp(data,"ab",2),"other owner survives cleanup");Check(Open(owner)==a,"recycled slot belongs to new owner");Check(fs->CloseFile(a,other)==-1,"stale foreign handle denied");
 }else if(!strcmp(name,"finish_cleanup")){
  Open(owner);Open(owner);Fault("");try{owner->Finish();Check(false,"Finish must notify");}catch(const Finished&){}Clear();Check(closeCalls==2&&Fds()==baseline,"actual Finish closes handles before notification");Check(owner->space==NULL,"Finish releases address space too");
 }else if(!strcmp(name,"destructor_cleanup")||!strcmp(name,"cleanup_close_error")){
  Open(owner);Open(other);Fault(!strcmp(name,"cleanup_close_error")?"close_error":"");delete fs;kernel->fileSystem=NULL;Clear();Check(closeCalls==2&&Fds()==baseline,"destructor releases every owner despite close errors");
 }else if(!strcmp(name,"close_error")||!strcmp(name,"close_eintr")||!strcmp(name,"double_close")){
  int id=Open(owner);Fault(!strcmp(name,"double_close")?"":name);Check(Trap(SC_Close,id)==(!strcmp(name,"double_close")?1:-1),"close status propagated");Check(closeCalls==1,"close exactly once");Check(Trap(SC_Close,id)==-1&&closeCalls==1,"closed handle consumed even on error");Clear();Check(Fds()==baseline,"close releases descriptor");Check(Open(owner)==id,"slot reusable after close error");
 }else if(!strcmp(name,"invalid_ids")){
  int ids[]={-1,0,21,INT_MAX};Fault("");for(int id:ids)Check(Trap(SC_Read,96,0,id)==-1&&Trap(SC_Write,80,0,id)==-1&&Trap(SC_Close,id)==-1,"bad ID rejected");Check(closeCalls==0,"invalid close has no host side effect");Clear();
 }else if(!strcmp(name,"length_eintr")||!strcmp(name,"length_host_error")){
  OpenFile*file=fs->Open(const_cast<char*>("data.bin"));Fault(!strcmp(name,"length_eintr")?"length_eintr":"length_error");Check(file->Length()==(!strcmp(name,"length_eintr")?6:-1),"length status and EINTR");Clear();Check(file->Length()==6,"length recovers");delete file;
 }else if(!strcmp(name,"length_error")){
  int fd=StubOpen(const_cast<char*>("data.bin"),false);OpenFile file(fd);Check(file.Length()==6,"length");__real_close(fd);Check(file.Length()==-1,"length failure recoverable");Check(file.CloseHandle()==-1,"bad close recoverable");
 }else if(!strcmp(name,"offset_limits")){
  OpenFile*file=fs->Open(const_cast<char*>("data.bin"));char b[4];Check(file->ReadAt(b,2,INT_MAX)==-1&&file->WriteAt(b,2,INT_MAX)==-1&&file->ReadAt(b,1,-1)==-1&&file->WriteAt(b,-1,0)==-1,"reject offset overflow and negatives");Check(file->Read(b,2)==2&&!memcmp(b,"ab",2),"bad offsets preserve sequential position");delete file;
 }else if(!strcmp(name,"readat_length")){
  OpenFile*file=fs->Open(const_cast<char*>("data.bin"));char b[4];Check(file->Read(b,2)==2&&!memcmp(b,"ab",2),"initial read");Check(file->Length()==6&&file->ReadAt(b,2,4)==2&&!memcmp(b,"ef",2),"positional read");Check(file->Read(b,2)==2&&!memcmp(b,"cd",2),"Length and ReadAt preserve logical position");delete file;
 }else{
  int id=Open(owner);bool writing=strstr(name,"write")!=NULL;const char*kind=name;
  if(!strcmp(name,"read_seek_error")||!strcmp(name,"write_seek_error"))kind="seek_error";
  if(!strcmp(name,"read_seek_eintr")||!strcmp(name,"write_seek_eintr"))kind="seek_eintr";
  Fault(kind);int count=Trap(writing?SC_Write:SC_Read,writing?80:96,4,id);Clear();
  int expected=strstr(name,"error")?-1:!strcmp(name,"zero_write")?0:strstr(name,"short")?2:4;
  Check(count==expected,"actual count/error propagated");
  if(!writing){Check(!memcmp(mem+96,expected>0?(expected==2?"ab!!!!!!":"abcd!!!!"):"!!!!!!!!",8),"only returned bytes copied out");}
  if(writing){FILE*result=fopen("data.bin","rb");char actual[6];Check(result&&fread(actual,1,6,result)==6,"result file");fclose(result);char wanted[6]={'a','b','c','d','e','f'};if(expected>0)memcpy(wanted,"WXYZ",expected);Check(!memcmp(actual,wanted,6),"only actual written bytes change file");}
  char next[2];Check(fs->ReadFile(next,2,id,owner)==2,"read after fault");int at=expected>0?expected:0;Check(!memcmp(next,"abcdef"+at,2),"position advances only by successful byte count");
 }
 Clear();if(kernel->fileSystem)delete kernel->fileSystem;puts("PASS");return 0;
}
