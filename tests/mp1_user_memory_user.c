#include "syscall.h"
static char names[384], outgoing[640], incoming[640], longName[257];
static int Stop(int code) { PrintInt(code==0?9902:-code); Halt(); return code; }
int main() {
    char base[]="mp1-bounds.bin";
    char *name=names+256-((unsigned int)names%128)-4;
    char *source=outgoing+256-((unsigned int)outgoing%128)-7;
    char *destination=incoming+256-((unsigned int)incoming%128)-7;
    int i,fd;
    for(i=0;i<sizeof base;i++) name[i]=base[i];
    for(i=0;i<300;i++) source[i]=(char)(1+i%120);
    for(i=0;i<350;i++) destination[i]='u';
    for(i=0;i<256;i++) longName[i]='x';
    longName[256]=0;
    if(Create((char*)-1)!=0 || Open((char*)-1)!=-1) return Stop(1);
    if(Create(longName)!=0 || Open(longName)!=-1) return Stop(2);
    if(Create(name)!=1) return Stop(3);
    fd=Open(name); if(fd<0) return Stop(4);
    if(Write((char*)-1,4,fd)!=-1 || Write(source,0x7fffffff,fd)!=-1) return Stop(5);
    if(Read(destination,-1,fd)!=-1 || Write(source,-1,fd)!=-1) return Stop(6);
    if(Read((char*)-1,0,fd)!=0 || Write((char*)-1,0,fd)!=0) return Stop(7);
    MSG((char*)-1); /* Invalid diagnostic strings must return without halting. */
    if(Write(source,300,fd)!=300 || Close(fd)!=1) return Stop(8);
    fd=Open(name); if(fd<0) return Stop(9);
    if(Read((char*)16382,4,fd)!=-1) return Stop(10);
    if(Read(destination,350,fd)!=300) return Stop(11);
    for(i=0;i<300;i++) if(source[i]!=destination[i]) return Stop(12);
    for(i=300;i<350;i++) if(destination[i]!='u') return Stop(13);
    if(Close(fd)!=1) return Stop(14);
    return Stop(0);
}
