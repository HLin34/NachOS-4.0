// Real FileSystem/OpenFile/sysdep code, with deterministic host-I/O fault injection.
#include "filesys.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

Debug *debug;
static int target=-1, fault=0, closeCalls=0;
enum { NONE, SHORT_IO, INTERRUPT_ONCE, HARD_ERROR, CLOSE_ERROR, CLOSE_INTERRUPT };
extern "C" {
ssize_t __real_read(int,void*,size_t);
ssize_t __real_write(int,const void*,size_t);
ssize_t __real_pread(int,void*,size_t,off_t);
ssize_t __real_pwrite(int,const void*,size_t,off_t);
int __real_close(int);
static int Error(int fd) {
    if(fd!=target) return 0;
    if(fault==INTERRUPT_ONCE) { fault=NONE; errno=EINTR; return 1; }
    if(fault==HARD_ERROR) { errno=EIO; return 1; }
    return 0;
}
static size_t Count(int fd,size_t size) {
    if(fd==target && fault==SHORT_IO) { fault=NONE; return size>3?3:size; }
    return size;
}
ssize_t __wrap_read(int fd,void *buffer,size_t size) {
    if(Error(fd)) return -1;
    return __real_read(fd,buffer,Count(fd,size));
}
ssize_t __wrap_write(int fd,const void *buffer,size_t size) {
    if(Error(fd)) return -1;
    return __real_write(fd,buffer,Count(fd,size));
}
ssize_t __wrap_pread(int fd,void *buffer,size_t size,off_t offset) {
    if(Error(fd)) return -1;
    return __real_pread(fd,buffer,Count(fd,size),offset);
}
ssize_t __wrap_pwrite(int fd,const void *buffer,size_t size,off_t offset) {
    if(Error(fd)) return -1;
    return __real_pwrite(fd,buffer,Count(fd,size),offset);
}
int __wrap_close(int fd) {
    if(fd==target) {
        closeCalls++;
        if(fault==CLOSE_ERROR || fault==CLOSE_INTERRUPT) {
            int error=fault==CLOSE_INTERRUPT?EINTR:EIO;
            fault=NONE;
            __real_close(fd); // Linux-style completed close reporting an error.
            errno=error;
            return -1;
        }
    }
    return __real_close(fd);
}
}
static void Check(bool good,const char *message) {
    if(!good) { fprintf(stderr,"FAIL: %s\n",message); exit(1); }
}
static int Fds() {
    DIR *d=opendir("/proc/self/fd"); Check(d!=NULL,"descriptor inventory");
    int count=0; while(readdir(d)) count++;
    closedir(d); return count;
}
static int FixtureFd() {
    struct stat wanted,actual;
    Check(stat("sample.bin",&wanted)==0,"fixture stat");
    for(int fd=0;fd<1024;fd++) if(fstat(fd,&actual)==0 &&
        wanted.st_dev==actual.st_dev && wanted.st_ino==actual.st_ino) return fd;
    Check(false,"fixture descriptor not found"); return -1;
}
int main(int argc,char **argv) {
    Check(argc==2,"test case required");
    char flags[]=""; debug=new Debug(flags);
    char name[]="sample.bin", absent[]="absent.bin", bad[]="missing-parent/new.bin";
    int initial=open(name,O_CREAT|O_TRUNC|O_RDWR,0600);
    Check(initial>=0 && write(initial,"abcdefghij",10)==10,"fixture creation"); close(initial);
    const char *test=argv[1]; char buffer[32]={0};
    if(strcmp(test,"full_table_no_leak")==0) {
        FileSystem fs;
        for(int i=0;i<20;i++) Check(fs.OpenAFile(name)==i,"fill all slots");
        int before=Fds();
        for(int i=0;i<200;i++) Check(fs.OpenAFile(name)==-1,"reject overflow");
        Check(Fds()==before,"full-table rejection leaked descriptors");
        for(int i=0;i<20;i++) Check(fs.CloseFile(i)==1,"close populated slot");
    } else if(strcmp(test,"destructor_cleanup")==0) {
        int before=Fds(); FileSystem *fs=new FileSystem;
        for(int i=0;i<4;i++) Check(fs->OpenAFile(name)>=0,"open for destruction");
        delete fs; Check(Fds()==before,"filesystem destruction releases open handles");
    } else if(strcmp(test,"slot_reuse")==0) {
        FileSystem fs;
        for(int cycle=0;cycle<40;cycle++) {
            Check(fs.OpenAFile(name)==0,"reuse earliest slot");
            Check(fs.CloseFile(0)==1 && fs.CloseFile(0)==-1,"close and double close");
        }
    } else if(strcmp(test,"missing_open")==0) {
        FileSystem fs; int before=Fds();
        for(int i=0;i<40;i++) Check(fs.OpenAFile(absent)==-1,"missing file");
        Check(Fds()==before,"missing file does not leak");
    } else if(strcmp(test,"failed_create")==0) {
        FileSystem fs; Check(!fs.Create(bad),"Create failure returns false");
        Check(fs.Create(name),"valid create still works");
    } else if(strcmp(test,"invalid_id_and_double_close")==0) {
        FileSystem fs;
        Check(fs.ReadFile(buffer,1,-1)==-1 && fs.WriteFile(buffer,1,20)==-1,"invalid IDs");
        Check(fs.CloseFile(0)==-1,"unopened close");
        int id=fs.OpenAFile(name); Check(fs.CloseFile(id)==1 && fs.CloseFile(id)==-1,"double close");
    } else if(strcmp(test,"invalid_sizes_null")==0) {
        FileSystem fs; int id=fs.OpenAFile(name);
        Check(fs.WriteFile(buffer,-1,id)==-1 && fs.ReadFile(buffer,-1,id)==-1,"negative size");
        Check(fs.ReadFile(NULL,1,id)==-1 && fs.WriteFile(NULL,1,id)==-1,"null nonempty buffer");
        Check(fs.ReadFile(NULL,0,id)==0 && fs.WriteFile(NULL,0,id)==0,"empty buffer");
        Check(fs.ReadFile(buffer,2,id)==2 && memcmp(buffer,"ab",2)==0,"invalid operations preserve offset");
    } else if(strcmp(test,"close_failure")==0 || strcmp(test,"close_interrupted_no_retry")==0) {
        FileSystem fs; int id=fs.OpenAFile(name); target=FixtureFd(); closeCalls=0;
        fault=strcmp(test,"close_failure")==0?CLOSE_ERROR:CLOSE_INTERRUPT;
        Check(fs.CloseFile(id)==-1,"close error reported");
        Check(closeCalls==1,"close must not retry or double close");
        target=-1;
        Check(fs.CloseFile(id)==-1 && fs.OpenAFile(name)==id,"failed close clears ownership and slot");
    } else {
        int fd=open(name,O_RDWR); Check(fd>=0,"open raw fixture"); target=fd;
        if(strcmp(test,"write_readonly")==0) { close(fd); fd=open(name,O_RDONLY); target=fd; }
        if(strcmp(test,"write_dev_full")==0) { close(fd); fd=open("/dev/full",O_RDWR); target=fd; }
        Check(fd>=0,"test descriptor"); OpenFile file(fd);
        if(strcmp(test,"read_error_offset")==0) {
            Check(file.Read(buffer,2)==2,"initial read"); fault=HARD_ERROR;
            Check(file.Read(buffer,2)==-1,"read failure"); fault=NONE;
            Check(file.Read(buffer,2)==2 && memcmp(buffer,"cd",2)==0,"failed read preserves offset");
        } else if(strcmp(test,"write_error_offset")==0) {
            Check(file.Write(const_cast<char*>("AB"),2)==2,"initial write"); fault=HARD_ERROR;
            Check(file.Write(const_cast<char*>("XX"),2)==-1,"write failure"); fault=NONE;
            Check(file.Write(const_cast<char*>("CD"),2)==2,"recovery write");
            Check(file.ReadAt(buffer,4,0)==4 && memcmp(buffer,"ABCD",4)==0,"failed write preserves offset");
        } else if(strcmp(test,"short_read")==0) {
            fault=SHORT_IO;
            Check(file.Read(buffer,8)==3 && memcmp(buffer,"abc",3)==0,"actual short read count");
            Check(file.Read(buffer,2)==2 && memcmp(buffer,"de",2)==0,"advance only bytes read");
        } else if(strcmp(test,"short_write")==0) {
            fault=SHORT_IO;
            Check(file.Write(const_cast<char*>("ABCDEFGH"),8)==3,"actual short write count");
            Check(file.Write(const_cast<char*>("XY"),2)==2,"post-short write");
            Check(file.ReadAt(buffer,5,0)==5 && memcmp(buffer,"ABCXY",5)==0,"advance only bytes written");
        } else if(strcmp(test,"read_eintr")==0) {
            fault=INTERRUPT_ONCE;
            Check(file.Read(buffer,3)==3 && memcmp(buffer,"abc",3)==0,"retry interrupted read");
        } else if(strcmp(test,"write_eintr")==0) {
            fault=INTERRUPT_ONCE;
            Check(file.Write(const_cast<char*>("ABC"),3)==3,"retry interrupted write");
        } else if(strcmp(test,"write_readonly")==0 || strcmp(test,"write_dev_full")==0) {
            Check(file.Write(const_cast<char*>("XX"),2)==-1,"host write error is nonfatal");
        } else if(strcmp(test,"length_failure")==0) {
            close(fd); Check(file.Length()==-1,"invalid descriptor length failure");
        } else if(strcmp(test,"eof_offsets")==0) {
            Check(file.Read(buffer,20)==10 && file.Read(buffer,2)==0 && file.Read(buffer,2)==0,"EOF returns zero");
        } else if(strcmp(test,"random_access")==0) {
            Check(file.Read(buffer,2)==2,"sequential prefix");
            Check(file.Length()==10 && file.WriteAt(const_cast<char*>("ZZ"),2,6)==2,"length and positioned write");
            Check(file.Read(buffer,2)==2 && memcmp(buffer,"cd",2)==0,"positioned operation preserves sequential offset");
            Check(file.ReadAt(buffer,1,-1)==-1 && file.WriteAt(buffer,1,-1)==-1,"invalid file position");
        } else Check(false,"unknown case");
    }
    puts("PASS"); return 0;
}
