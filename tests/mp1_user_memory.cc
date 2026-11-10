// Exercise the actual exception handler and stub filesystem with fragmented maps.
#include "main.h"
#include "syscall.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

Kernel *kernel;
Debug *debug;
static void Check(bool value, const char *why) {
    if (!value) { fprintf(stderr,"FAIL: %s\n",why); exit(1); }
}
static TranslationEntry maps[2][8];
static void Context(int context) {
    kernel->machine->pageTable = maps[context];
    kernel->machine->pageTableSize = 8;
}
static unsigned int Physical(unsigned int address) {
    return kernel->machine->pageTable[address/PageSize].physicalPage*PageSize + address%PageSize;
}
static void Put(unsigned int address, const char *data, int size) {
    for(int i=0;i<size;i++) kernel->machine->mainMemory[Physical(address+i)]=data[i];
}
static void Expect(unsigned int address, const char *data, int size) {
    for(int i=0;i<size;i++) Check(kernel->machine->mainMemory[Physical(address+i)]==data[i],"user bytes");
}
static int Call(int type, int arg1=0, int arg2=0, int arg3=0) {
    Machine *m=kernel->machine;
    m->WriteRegister(PCReg,40); m->WriteRegister(NextPCReg,44);
    m->WriteRegister(2,type); m->WriteRegister(4,arg1);
    m->WriteRegister(5,arg2); m->WriteRegister(6,arg3);
    ExceptionHandler(SyscallException);
    Check(m->ReadRegister(PrevPCReg)==40 && m->ReadRegister(PCReg)==44 &&
          m->ReadRegister(NextPCReg)==48,"syscall returns with advanced PC");
    return m->ReadRegister(2);
}
int main(int argc,char **argv) {
    Check(argc==2,"case required");
    char name[]="syscall-test", flags[]=""; char *options[]={name};
    debug=new Debug(flags); void *storage=calloc(1,sizeof(Kernel)); Check(storage!=NULL,"kernel backing storage");
    kernel=new (storage) Kernel(1,options); kernel->Initialize();
    memset(kernel->machine->mainMemory,0xa5,MemorySize);
    for(int c=0;c<2;c++) for(int p=0;p<8;p++) {
        maps[c][p].virtualPage=p; maps[c][p].physicalPage=17+c*24+(p*3)%8;
        maps[c][p].valid=true; maps[c][p].readOnly=false;
        maps[c][p].use=false; maps[c][p].dirty=false;
    }
    Context(0);
    const char *test=argv[1];
    const char filename[]="sample.bin";
    char payload[300]; for(int i=0;i<300;i++) payload[i]=(char)(1+i%120);
    FILE *f=fopen(filename,"wb"); Check(f!=NULL,"fixture open");
    Check(fwrite(payload,1,300,f)==300,"fixture write"); fclose(f);
    char hostName[]="sample.bin";
    int fd=kernel->fileSystem->OpenAFile(hostName); Check(fd>=0,"fixture descriptor");
    if(strcmp(test,"filename_cross_page")==0) {
        const char created[]="created.bin"; Put(124,created,sizeof created);
        Check(Call(SC_Create,124)==1,"Create translated filename");
        int opened=Call(SC_Open,124); Check(opened>=0,"Open translated filename");
        Check(Call(SC_Close,opened)==1,"Close");
        FILE *createdFile=fopen(created,"rb"); Check(createdFile!=NULL,"created host file"); fclose(createdFile);
    } else if(strcmp(test,"write_cross_page")==0 || strcmp(test,"two_contexts")==0) {
        Put(121,payload,300);
        Check(Call(SC_Write,121,300,fd)==300,"Write length");
        FILE *readback=fopen(filename,"rb"); char actual[300];
        Check(fread(actual,1,300,readback)==300 && memcmp(actual,payload,300)==0,"host bytes"); fclose(readback);
        Check(maps[0][0].use && !maps[0][0].dirty,"copy-in marks use, not dirty");
        if(strcmp(test,"two_contexts")==0) {
            Context(1); int other=kernel->fileSystem->OpenAFile(hostName);
            Check(Call(SC_Read,121,300,other)==300,"second context Read"); Expect(121,payload,300);
            Context(0); Expect(121,payload,300);
        }
    } else if(strcmp(test,"read_cross_page")==0 || strcmp(test,"eof_tail")==0) {
        int requested=strcmp(test,"eof_tail")==0?350:300;
        Check(Call(SC_Read,121,requested,fd)==300,"Read actual count"); Expect(121,payload,300);
        if(requested==350) for(int i=300;i<350;i++)
            Check((unsigned char)kernel->machine->mainMemory[Physical(121+i)]==0xa5,"EOF tail untouched");
        Check(maps[0][0].dirty && maps[0][1].dirty,"copy-out marks dirty");
    } else if(strcmp(test,"bad_write_range")==0) {
        Check(Call(SC_Write,1000,40,fd)==-1,"invalid Write rejected");
        Check(Call(SC_Read,100,4,fd)==4,"failed Write did not move offset"); Expect(100,payload,4);
    } else if(strcmp(test,"bad_read_range")==0 || strcmp(test,"readonly_read")==0 ||
              strcmp(test,"invalid_page")==0 || strcmp(test,"negative_frame")==0) {
        int address=1000, count=40;
        if(strcmp(test,"readonly_read")==0) { maps[0][1].readOnly=true; address=121; count=30; }
        if(strcmp(test,"invalid_page")==0) { maps[0][1].valid=false; address=121; count=30; }
        if(strcmp(test,"negative_frame")==0) { maps[0][1].physicalPage=-1; address=121; count=30; }
        Check(Call(SC_Read,address,count,fd)==-1,"bad destination rejected");
        maps[0][1].readOnly=false; maps[0][1].valid=true; maps[0][1].physicalPage=20;
        Check(Call(SC_Read,100,4,fd)==4,"failed Read did not move offset"); Expect(100,payload,4);
    } else if(strcmp(test,"negative_size")==0) {
        Check(Call(SC_Read,100,-1,fd)==-1 && Call(SC_Write,100,-1,fd)==-1,"negative size");
    } else if(strcmp(test,"negative_pointer")==0) {
        Check(Call(SC_Read,-1,1,fd)==-1 && Call(SC_Write,-1,1,fd)==-1,"negative pointer");
        Check(Call(SC_Open,-1)==-1 && Call(SC_Create,-1)==0,"negative filename");
    } else if(strcmp(test,"zero_size")==0) {
        Check(Call(SC_Read,-1,0,fd)==0 && Call(SC_Write,-1,0,fd)==0,"zero bytes need no pointer");
    } else if(strcmp(test,"invalid_id")==0) {
        Check(Call(SC_Read,100,1,20)==-1 && Call(SC_Write,100,1,-1)==-1,"invalid file id");
    } else if(strcmp(test,"unterminated_filename")==0) {
        char data[256]; memset(data,'x',sizeof data); Put(121,data,256);
        Check(Call(SC_Open,121)==-1 && Call(SC_Create,121)==0,"bounded filename");
    } else if(strcmp(test,"filename_unmapped_tail")==0) {
        char data[24]; memset(data,'x',24); Put(1000,data,24);
        Check(Call(SC_Open,1000)==-1,"filename crosses address-space end");
    } else if(strcmp(test,"invalid_message")==0) {
        Check(Call(SC_MSG,-1)==-1,"invalid diagnostic message returns error");
    } else if(strcmp(test,"buffer_end_exact")==0) {
        Check(Call(SC_Read,1020,4,fd)==4,"buffer ending at virtual limit"); Expect(1020,payload,4);
        Check(Call(SC_Read,1024,1,fd)==-1 && Call(SC_Write,1024,1,fd)==-1,"buffer starting outside map");
    } else if(strcmp(test,"overflow_span")==0) {
        Check(Call(SC_Read,0x7fffffff,2,fd)==-1 && Call(SC_Write,0x7fffffff,2,fd)==-1,"large pointer");
        Check(Call(SC_Read,100,0x7fffffff,fd)==-1 && Call(SC_Write,100,0x7fffffff,fd)==-1,"oversized count");
    } else if(strcmp(test,"high_physical_frame")==0) {
        maps[0][0].physicalPage=NumPhysPages;
        Check(Call(SC_Read,100,4,fd)==-1 && Call(SC_Write,100,4,fd)==-1,"frame outside physical memory");
    } else if(strcmp(test,"missing_page_table")==0) {
        kernel->machine->pageTable=NULL;
        Check(Call(SC_Read,100,4,fd)==-1 && Call(SC_Write,100,4,fd)==-1,"absent mapping");
        Check(Call(SC_Open,100)==-1 && Call(SC_Create,100)==0,"absent string mapping");
    } else if(strcmp(test,"invalid_table_size")==0) {
        kernel->machine->pageTableSize=NumPhysPages+1;
        Check(Call(SC_Read,100,4,fd)==-1,"oversized page table");
        kernel->machine->pageTableSize=0;
        Check(Call(SC_Write,100,4,fd)==-1,"empty page table");
    } else if(strcmp(test,"filename_last_nul")==0) {
        unsigned int start=1024-sizeof filename;
        Put(start,filename,sizeof filename);
        int opened=Call(SC_Open,start);
        Check(opened>=0 && Call(SC_Close,opened)==1,"NUL at final mapped byte");
    } else if(strcmp(test,"filename_limit")==0) {
        char longName[257]; memset(longName,'a',256); longName[255]='\0';
        Put(121,longName,256); Check(Call(SC_Create,121)==1,"255-byte name accepted");
        longName[255]='a'; longName[256]='\0'; Put(121,longName,257);
        Check(Call(SC_Create,121)==0 && Call(SC_Open,121)==-1,"256-byte name rejected");
    } else if(strcmp(test,"rejected_read_no_partial")==0) {
        char sentinel[30]; memset(sentinel,'u',30); Put(121,sentinel,30);
        maps[0][1].valid=false;
        Check(Call(SC_Read,121,30,fd)==-1,"whole range rejected");
        for(int i=0;i<7;i++) Check(kernel->machine->mainMemory[Physical(121+i)]=='u',"valid prefix untouched");
        maps[0][1].valid=true;
        Check(Call(SC_Read,100,4,fd)==4,"file offset untouched"); Expect(100,payload,4);
    } else if(strcmp(test,"readonly_source_write")==0) {
        Put(121,payload,300); maps[0][0].readOnly=true; maps[0][1].readOnly=true;
        Check(Call(SC_Write,121,300,fd)==300,"read-only source is readable");
        Check(!maps[0][0].dirty && !maps[0][1].dirty,"copy-in does not dirty source");
    } else if(strcmp(test,"unterminated_message")==0) {
        char data[256]; memset(data,'x',256); Put(121,data,256);
        Check(Call(SC_MSG,121)==-1,"bounded diagnostic string");
    } else if(strcmp(test,"rejected_create_no_truncate")==0) {
        Put(121,filename,sizeof filename); maps[0][1].valid=false;
        Check(Call(SC_Create,121)==0,"bad filename tail rejected");
        FILE *original=fopen(filename,"rb"); char actual[300];
        Check(fread(actual,1,300,original)==300 && memcmp(actual,payload,300)==0,"failed Create leaves existing file intact"); fclose(original);
    } else { Check(false,"unknown case"); }
    puts("PASS");
    return 0;
}
