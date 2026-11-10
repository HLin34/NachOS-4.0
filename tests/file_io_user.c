#include "syscall.h"
static int Finish(int code) {
#ifdef MP1_COURSE_MODE
    if(code==0) { PrintInt(9901); Halt(); }
#endif
    Exit(code); return code;
}
int main() {
    char name[]="io-recovery.bin", missing[]="missing-parent/fail.bin", full[]="/dev/full";
    char data[]="ABCD", readback[4];
    int handles[20],i,fd;
    if(Create(missing)!=0 || Open(missing)!=-1) return Finish(1);
    if(Create(name)!=1) return Finish(2);
    for(i=0;i<20;i++) { handles[i]=Open(name); if(handles[i]!=i) return Finish(3); }
    for(i=0;i<30;i++) if(Open(name)!=-1) return Finish(4);
    for(i=0;i<20;i++) if(Close(handles[i])!=1) return Finish(5);
    fd=Open(full);
    if(fd<0 || Write(data,4,fd)!=-1 || Close(fd)!=1) return Finish(6);
    fd=Open(name);
    if(fd!=0) return Finish(7);
    if(Read(readback,-1,fd)!=-1 || Write(data,-1,fd)!=-1) return Finish(8);
    if(Write(data,4,fd)!=4 || Read(readback,4,fd)!=0 || Close(fd)!=1) return Finish(9);
    fd=Open(name);
    if(fd<0 || Read(readback,4,fd)!=4) return Finish(10);
    for(i=0;i<4;i++) if(readback[i]!=data[i]) return Finish(11);
    if(Close(fd)!=1 || Close(fd)!=-1) return Finish(12);
    return Finish(0);
}
