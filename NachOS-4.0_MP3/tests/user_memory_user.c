#include "syscall.h"
#ifndef TEST_ID
#define TEST_ID 1
#endif
char names[512],output[300],input[300],longname[257];
int main() {
    int i,id;unsigned int offset;char *name;char *text;
#if TEST_ID == 1
    text="syscall-one.bin";
#else
    text="syscall-two.bin";
#endif
    offset=(128-((unsigned int)names%128))%128;
    name=names+offset+123;
    for(i=0;text[i]!=0;i++)name[i]=text[i];name[i]=0;
    PrintInt(100+TEST_ID);
    if(Create((char*)-1)!=-1)Exit(-1);
    if(Open((char*)0x7fffffff)!=-1)Exit(-2);
    for(i=0;i<256;i++)longname[i]='x';longname[256]=0;
    if(Open(longname)!=-1)Exit(-3);
    MSG((char*)-1);MSG(longname); /* Invalid messages must return, never halt. */
    if(Create(name)!=1)Exit(-4);
    id=Open(name);if(id<1)Exit(-5);
    if(Read((char*)-1,1,id)!=-1)Exit(-6);
    if(Write((char*)-1,1,id)!=-1)Exit(-7);
    if(Read(input,-1,id)!=-1)Exit(-8);
    if(Write(output,0x7fffffff,id)!=-1)Exit(-9);
    if(Read((char*)-1,0,id)!=0||Write((char*)-1,0,id)!=0)Exit(-10);
    for(i=0;i<300;i++)output[i]=(i*17+TEST_ID)%97+32;
    if(Write(output,300,id)!=300||Close(id)!=1)Exit(-11);
    id=Open(name);if(id<1)Exit(-12);
    if(Read((char*)-1,1,id)!=-1)Exit(-13);
    if(Read(input,300,id)!=300)Exit(-14);
    for(i=0;i<300;i++)if(input[i]!=output[i])Exit(-15);
    input[0]=88;if(Read(input,1,id)!=0||input[0]!=88)Exit(-16);
    if(Close(id)!=1)Exit(-17);
    PrintInt(200+TEST_ID);Exit(60+TEST_ID);
}
