/* Real MIPS programs intentionally leave descriptors open for Finish cleanup. */
#include "syscall.h"
#define STOP(code) do { Exit(code); return code; } while (0)
#ifndef TEST_ID
#define TEST_ID 0
#endif
#if TEST_ID == 0
#define NAME "io-capacity.bin"
#define COUNT 20
#else
#if TEST_ID == 1
#define NAME "io-one.bin"
#else
#define NAME "io-two.bin"
#endif
#define COUNT 2
#endif
int main(){
 int ids[20],i,j,mine;char byte='A'+TEST_ID;
 if(Create(NAME)!=1)STOP(-101);
 for(i=0;i<COUNT;i++){ids[i]=Open(NAME);if(ids[i]<1)STOP(-102);}
#if TEST_ID == 0
 if(Open(NAME)!=-1)STOP(-103);
#else
 PrintInt(300+TEST_ID); /* Block with live handles, allowing the peer to run. */
 for(i=1;i<=20;i++){
  mine=0;for(j=0;j<COUNT;j++)if(ids[j]==i)mine=1;
  if(!mine&&(Close(i)!=-1||Write(&byte,1,i)!=-1))STOP(-104);
 }
#endif
 for(i=0;i<COUNT;i++)if(Write(&byte,1,ids[i])!=1)STOP(-105);
#if TEST_ID != 0
 PrintInt(400+TEST_ID);
#endif
 STOP(70+TEST_ID);
}
