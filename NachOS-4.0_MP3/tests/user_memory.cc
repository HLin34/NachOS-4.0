// Link real syscall/kernel objects; intercept unsafe baseline host effects.
#include <iostream>
#include <sstream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include "main.h"
#include "syscall.h"
Kernel *kernel;Debug *debug;
struct Halted {};
static bool watch=false,block=false;
static int opens=0,reads=0,writes=0,halts=0;
extern "C" int __real_open(const char*,int,...);
extern "C" ssize_t __real_read(int,void*,size_t);
extern "C" ssize_t __real_write(int,const void*,size_t);
extern "C" int __wrap_open(const char *path,int flags,...) {
    mode_t mode=0;if(flags&O_CREAT){va_list a;va_start(a,flags);mode=va_arg(a,int);va_end(a);}
    if(watch){++opens;if(block){errno=EINVAL;return -1;}}
    return __real_open(path,flags,mode);
}
extern "C" ssize_t __wrap_read(int fd,void *buffer,size_t n) {
    if(watch&&fd>2){++reads;if(block){errno=EINVAL;return -1;}}
    return __real_read(fd,buffer,n);
}
extern "C" ssize_t __wrap_write(int fd,const void *buffer,size_t n) {
    if(watch&&fd>2){++writes;if(block){errno=EINVAL;return -1;}}
    return __real_write(fd,buffer,n);
}
extern "C" void __wrap__ZN9Interrupt4HaltEv(Interrupt*) {++halts;throw Halted();}
static void Check(bool value,const char *why){if(!value){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
static unsigned Limit(){return kernel->machine->pageTableSize*PageSize;}
static void Put(unsigned address,const char *data,unsigned n){
    for(unsigned i=0;i<n;i++){TranslationEntry &e=kernel->machine->pageTable[(address+i)/PageSize];kernel->machine->mainMemory[e.physicalPage*PageSize+(address+i)%PageSize]=data[i];}
}
static std::vector<char> Get(unsigned address,unsigned n){std::vector<char> bytes(n);for(unsigned i=0;i<n;i++){TranslationEntry &e=kernel->machine->pageTable[(address+i)/PageSize];bytes[i]=kernel->machine->mainMemory[e.physicalPage*PageSize+(address+i)%PageSize];}return bytes;}
static void Flags(){for(unsigned p=0;p<kernel->machine->pageTableSize;p++)kernel->machine->pageTable[p].use=kernel->machine->pageTable[p].dirty=false;}
static int Call(int type,int a=0,int b=0,int c=0,bool invalid=false,int next=44){
    Machine *m=kernel->machine;m->WriteRegister(2,type);m->WriteRegister(4,a);m->WriteRegister(5,b);m->WriteRegister(6,c);
    m->WriteRegister(PrevPCReg,36);m->WriteRegister(PCReg,40);m->WriteRegister(NextPCReg,next);
    opens=reads=writes=0;watch=true;block=invalid;bool stopped=false;
    try{ExceptionHandler(SyscallException);}catch(const Halted&){stopped=true;}
    watch=false;block=false;
    Check(!stopped,"invalid request must not halt; normal file calls must return");
    Check(m->ReadRegister(PrevPCReg)==40&&m->ReadRegister(PCReg)==next&&m->ReadRegister(NextPCReg)==next+4,"one PC advance uses saved NextPC");
    return m->ReadRegister(2);
}
static void NoIO(){Check(opens==0&&reads==0&&writes==0,"invalid input must not reach host file I/O");}
static std::vector<char> Input(){std::vector<char> data(600);for(unsigned i=0;i<data.size();i++)data[i]=static_cast<char>((i*7+1)%251);return data;}
static int File(const char *name,const std::vector<char> &data){FILE *f=fopen(name,"wb");Check(f!=NULL,"fixture file");Check(fwrite(data.data(),1,data.size(),f)==data.size(),"fixture contents");fclose(f);int id=kernel->fileSystem->OpenAFile(const_cast<char*>(name),kernel->currentThread);Check(id>0,"fixture handle");return id;}
static std::vector<char> Host(const char *name){FILE*f=fopen(name,"rb");Check(f!=NULL,"host result");std::vector<char>v;int c;while((c=fgetc(f))!=EOF)v.push_back(c);fclose(f);return v;}
static void Reject(int type,int address,int size=0,int id=0){
    std::ostringstream text;std::streambuf *old=NULL;
    if(type==SC_MSG)old=std::cout.rdbuf(text.rdbuf());
    int result=Call(type,address,size,id,true);
    if(old)std::cout.rdbuf(old);
    Check(result==-1,"bad user argument returns -1");NoIO();
    if(type==SC_MSG)Check(text.str().empty(),"invalid MSG produces no partial output");
}
int main(int argc,char **argv){
    Check(argc==3,"case and image");const char *name=argv[1];debug=new Debug(const_cast<char*>(""));kernel=new Kernel(0,NULL);
    kernel->stats=new Statistics;kernel->interrupt=new Interrupt;kernel->scheduler=new Scheduler;kernel->machine=new Machine(false);kernel->fileSystem=new FileSystem;
    kernel->currentThread=new Thread(const_cast<char*>("fixture"),0);kernel->currentThread->setStatus(RUNNING);
    kernel->numAvailPhysPage=0;for(int p=0;p<NumPhysPages;p++){kernel->usedPhysPage[p]=p%2==0;if(!kernel->usedPhysPage[p])++kernel->numAvailPhysPage;}
    memset(kernel->machine->mainMemory,0xa5,MemorySize);
    AddrSpace *space=new AddrSpace;Check(space->Load(argv[2]),"real nonidentity mapping");kernel->currentThread->space=space;space->RestoreState();
    TranslationEntry *table=kernel->machine->pageTable;unsigned pages=kernel->machine->pageTableSize,limit=Limit();
    std::vector<char> data=Input();
    if(!strcmp(name,"filename_cross_page")||!strcmp(name,"filename_max")){
        std::string filename=!strcmp(name,"filename_max")?std::string(251,'n')+".bin":"named.bin";
        Put(123,filename.c_str(),filename.size()+1);Check(Call(SC_Create,123)==1,"copied bounded filename creates");Check(Host(filename.c_str()).empty(),"intended host filename");
    }else if(!strcmp(name,"open_cross_page")){
        File("sample.bin",data);Put(123,"sample.bin",11);Check(Call(SC_Open,123)>0,"copied bounded name opens");
    }else if(!strcmp(name,"negative_name"))Reject(SC_Create,-1);
    else if(!strcmp(name,"name_at_limit"))Reject(SC_Open,limit);
    else if(!strcmp(name,"far_name"))Reject(SC_Create,2147483647);
    else if(!strcmp(name,"empty_name")){char zero=0;Put(limit-1,&zero,1);Reject(SC_Create,limit-1);}
    else if(!strcmp(name,"unterminated_name")||!strcmp(name,"name_too_long")){
        std::vector<char> bytes(257,'a');if(!strcmp(name,"name_too_long"))bytes[256]=0;Put(64,bytes.data(),bytes.size());Reject(SC_Open,64);
    }else if(!strcmp(name,"name_invalid_page")){
        Put(123,"named.bin",10);table[1].valid=false;Reject(SC_Create,123);
    }else if(!strcmp(name,"name_negative_frame")||!strcmp(name,"name_upper_frame")){
        table[0].physicalPage=!strcmp(name,"name_negative_frame")?-1:NumPhysPages;Reject(SC_Open,64);
    }else if(!strcmp(name,"missing_space")){kernel->currentThread->space=NULL;Reject(SC_Open,64);}
    else if(!strcmp(name,"missing_table")){kernel->machine->pageTable=NULL;Reject(SC_Open,64);}
    else if(!strcmp(name,"empty_table")){kernel->machine->pageTableSize=0;Reject(SC_Open,64);}
    else if(!strcmp(name,"oversized_table")){kernel->machine->pageTableSize=NumPhysPages+1;Reject(SC_Open,64);}
    else if(!strcmp(name,"write_cross_page")||!strcmp(name,"write_readonly_source")){
        std::vector<char> payload(data.begin(),data.begin()+300);int id=File("result.bin",{});Put(121,payload.data(),payload.size());Flags();
        if(!strcmp(name,"write_readonly_source"))table[0].readOnly=true;
        Check(Call(SC_Write,121,300,id)==300,"paged write count");Check(Host("result.bin")==payload,"exact paged output");
        for(unsigned p=0;p<4;p++)Check(table[p].use&&!table[p].dirty,"write reads user pages without dirtying");
    }else if(!strcmp(name,"read_cross_page")){
        int id=File("input.bin",data);Flags();Check(Call(SC_Read,121,300,id)==300,"paged read count");
        Check(Get(121,300)==std::vector<char>(data.begin(),data.begin()+300),"exact paged input");for(unsigned p=0;p<4;p++)Check(table[p].use&&table[p].dirty,"actual read targets dirty");
    }else if(!strcmp(name,"eof_tail")){
        int id=File("input.bin",{'x','y','z'});std::vector<char> fill(20,'!');Put(125,fill.data(),fill.size());Flags();
        Check(Call(SC_Read,125,20,id)==3,"short EOF count");fill[0]='x';fill[1]='y';fill[2]='z';Check(Get(125,20)==fill,"EOF tail unchanged");Check(table[0].dirty&&!table[1].dirty,"only transferred pages dirty");
    }else if(!strcmp(name,"invalid_later_write")||!strcmp(name,"invalid_later_read")||!strcmp(name,"readonly_read")){
        bool reading=strcmp(name,"invalid_later_write")!=0;int id=File("input.bin",reading?data:std::vector<char>());
        std::vector<char> fill(300,'!');Put(121,fill.data(),fill.size());Flags();
        TranslationEntry saved=table[2];if(!strcmp(name,"readonly_read"))table[2].readOnly=true;else table[2].valid=false;
        Reject(reading?SC_Read:SC_Write,121,300,id);Check(!table[0].use&&!table[0].dirty,"failed whole-range validation leaves earlier pages untouched");
        table[2]=saved;Check(Get(121,300)==fill,"invalid buffer not partially overwritten");
        if(reading){char first[3];Check(kernel->fileSystem->OpenFileTable[id-1]->Read(first,3)==3&&memcmp(first,data.data(),3)==0,"rejected read preserves file offset");}
        else Check(Host("input.bin").empty(),"rejected write preserves file");
    }else if(!strcmp(name,"negative_count")||!strcmp(name,"oversized_count")||!strcmp(name,"huge_count")||!strcmp(name,"buffer_at_limit")||!strcmp(name,"buffer_tail")||!strcmp(name,"far_buffer")){
        int id=File("input.bin",data);int address=64,size=1;
        if(!strcmp(name,"negative_count"))size=-1;
        if(!strcmp(name,"oversized_count"))size=MemorySize+1;
        if(!strcmp(name,"huge_count"))size=2147483647;
        if(!strcmp(name,"buffer_at_limit"))address=limit;
        if(!strcmp(name,"buffer_tail")){address=limit-1;size=2;}
        if(!strcmp(name,"far_buffer")){address=2147483630;size=64;}
        Reject(SC_Read,address,size,id);
    }else if(!strcmp(name,"negative_buffer_frame")||!strcmp(name,"upper_buffer_frame")){
        int id=File("input.bin",data);table[0].physicalPage=!strcmp(name,"negative_buffer_frame")?-1:NumPhysPages;Reject(SC_Read,64,10,id);
    }else if(!strcmp(name,"zero_read")||!strcmp(name,"zero_write")){
        int id=File("input.bin",data);Flags();Check(Call(!strcmp(name,"zero_read")?SC_Read:SC_Write,-1,0,id)==0,"empty span does not dereference pointer");
        char first[3];Check(kernel->fileSystem->OpenFileTable[id-1]->Read(first,3)==3&&memcmp(first,data.data(),3)==0,"empty transfer preserves file offset");
        for(unsigned p=0;p<pages;p++)Check(!table[p].use&&!table[p].dirty,"empty span touches no page");
    }else if(!strcmp(name,"invalid_descriptor")){Check(Call(SC_Read,-1,0,99)==-1,"empty request still checks descriptor");NoIO();}
    else if(!strcmp(name,"pc_delay_slot")){Check(Call(SC_Add,7,8,0,false,80)==15,"scalar Add result at a nonsequential NextPC");NoIO();}
    else if(!strcmp(name,"message_negative"))Reject(SC_MSG,-1);
    else if(!strcmp(name,"message_unterminated")){std::vector<char>text(256,'m');Put(64,text.data(),text.size());Reject(SC_MSG,64);}
    else if(!strcmp(name,"message_cross_page")||!strcmp(name,"message_last_zero")){
        const char *message=!strcmp(name,"message_last_zero")?"":"MP3 bounded message";unsigned address=!strcmp(name,"message_last_zero")?limit-1:125;Put(address,message,strlen(message)+1);
        Machine *m=kernel->machine;m->WriteRegister(2,SC_MSG);m->WriteRegister(4,address);m->WriteRegister(PCReg,40);m->WriteRegister(NextPCReg,44);
        std::ostringstream text;std::streambuf *old=std::cout.rdbuf(text.rdbuf());bool stopped=false;
        try{ExceptionHandler(SyscallException);}catch(const Halted&){stopped=true;}std::cout.rdbuf(old);
        Check(stopped&&halts==1&&text.str()==std::string(message)+"\n","valid bounded MSG retains output and halt");Check(m->ReadRegister(PCReg)==40&&m->ReadRegister(NextPCReg)==44,"nonreturning MSG does not advance PC");
    }else Check(false,"unknown case");
    puts("PASS");return 0;
}
