#include "syscall.h"
#ifndef TEST_ID
#define TEST_ID 1
#endif

static char names[384];
static char outgoing[640];
static char incoming[640];

static int Finish(int code) { Exit(code); return code; }

int main() {
    int i, fd, offset;
    char *name, *source, *destination;
    char templateName[] = "syscall0.bin";
    offset = 256 - ((unsigned int)names % 128) - 4;
    name = names + offset;
    for (i = 0; i < 13; i++) name[i] = templateName[i];
    name[7] = '0' + TEST_ID;
    source = outgoing + 256 - ((unsigned int)outgoing % 128) - 7;
    destination = incoming + 256 - ((unsigned int)incoming % 128) - 7;
    for (i = 0; i < 300; i++) source[i] = (char)(1 + (i + TEST_ID) % 120);
    if (Create(name) != 1) return Finish(1);
    fd = Open(name);
    if (fd < 0) return Finish(2);
    if (Write(source, 300, fd) != 300) return Finish(3);
    if (Close(fd) != 1) return Finish(4);
    fd = Open(name);
    if (fd < 0) return Finish(5);
    if (Read(destination, 300, fd) != 300) return Finish(6);
    if (Close(fd) != 1) return Finish(7);
    for (i = 0; i < 300; i++) if (source[i] != destination[i]) return Finish(8);
    PrintInt(7000 + TEST_ID);
    return Finish(0);
}
