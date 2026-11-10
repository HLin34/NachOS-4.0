// Link against the real MP2 kernel objects, replacing only the driver main.
#include "main.h"
#include "addrspace.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

Kernel *kernel;
Debug *debug;

static void Check(bool condition, const char *message) {
    if (!condition) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}

int main(int argc, char **argv) {
    Check(argc == 3, "usage: memory-test init|load fixture-prefix");
    char name[] = "memory-test";
    char *options[] = {name};
    char flags[] = "";
    debug = new Debug(flags);
    kernel = new Kernel(1, options);
    // A nonzero precondition makes incomplete initialization deterministic.
    for (int i = 0; i < NumPhysPages; i++) kernel->frameTable[i] = true;
    kernel->Initialize();
    if (strcmp(argv[1], "init") == 0) {
        for (int i = 0; i < NumPhysPages; i++)
            Check(!kernel->frameTable[i], "frame table was not fully initialized");
        Check(kernel->NumFreeFrame == NumPhysPages, "initial free-frame count");
        puts("PASS: initialization");
        return 0;
    }
    // Loader tests isolate loading from the independent initialization defect.
    bool reserved[NumPhysPages];
    int originalFree = 0;
    for (int i = 0; i < NumPhysPages; i++) {
        reserved[i] = i % 2 == 0;
        kernel->frameTable[i] = reserved[i];
        if (!reserved[i]) originalFree++;
    }
    kernel->NumFreeFrame = originalFree;
    memset(kernel->machine->mainMemory, 0xa5, MemorySize);
    if (strcmp(argv[1], "reject") == 0) {
        for (int cycle = 0; cycle < 3; cycle++) {
            AddrSpace *space = new AddrSpace;
            Check(!space->Load(argv[2]), "truncated payload rejected");
            Check(kernel->NumFreeFrame == originalFree, "failed load returns frames");
            delete space;
            Check(kernel->NumFreeFrame == originalFree, "failed load not freed twice");
            for (int i = 0; i < NumPhysPages; i++)
                Check(kernel->frameTable[i] == reserved[i], "failed load restores bitmap");
        }
        puts("PASS: truncated payload cleanup");
        return 0;
    }
    char expectedName[1024];
    snprintf(expectedName, sizeof expectedName, "%s.expected", argv[2]);
    FILE *expected = fopen(expectedName, "rb");
    Check(expected != NULL, "expected bytes open");
    unsigned int imageSize;
    Check(fread(&imageSize, sizeof imageSize, 1, expected) == 1, "expected length");
    unsigned char *image = new unsigned char[imageSize];
    Check(fread(image, 1, imageSize, expected) == imageSize, "expected contents");
    fclose(expected);
    // Repeated allocation checks frame return and stale-data clearing.
    for (int cycle = 0; cycle < 3; cycle++) {
        AddrSpace *space = new AddrSpace;
        Check(space->Load(argv[2]), "Load succeeds");
        unsigned int pages = divRoundUp(imageSize + UserStackSize, PageSize);
        Check(kernel->NumFreeFrame == originalFree - (int)pages, "allocation count");
        space->RestoreState();
        for (unsigned int vpn = 0; vpn < pages; vpn++) {
            TranslationEntry entry = kernel->machine->pageTable[vpn];
            Check(entry.valid && !entry.dirty, "valid and clean page flags");
            Check(!reserved[entry.physicalPage], "reserved frame not allocated");
            for (unsigned int previous = 0; previous < vpn; previous++)
                Check(kernel->machine->pageTable[previous].physicalPage != entry.physicalPage,
                      "unique physical frame per page");
        }
        for (unsigned int v = 0; v < pages * PageSize; v++) {
            unsigned int physical;
            Check(space->Translate(v, &physical, false) == NoException, "read translation");
            unsigned char want = v < imageSize ? image[v] : 0;
            Check((unsigned char)kernel->machine->mainMemory[physical] == want,
                  "loaded bytes, gaps, BSS or stack differ");
        }
        // Optional protection oracle: one byte per expected page.
        char protectionName[1024];
        snprintf(protectionName, sizeof protectionName, "%s.protection", argv[2]);
        FILE *protection = fopen(protectionName, "rb");
        Check(protection != NULL, "protection oracle open");
        for (unsigned int vpn = 0; vpn < pages; vpn++) {
            int want = fgetc(protection);
            Check(want == 0 || want == 1, "protection oracle length");
            Check(kernel->machine->pageTable[vpn].readOnly == (want == 1), "page protection");
            if (want == 1) {
                unsigned int physical;
                Check(space->Translate(vpn * PageSize, &physical, true) == ReadOnlyException,
                      "write to read-only page rejected");
            }
        }
        fclose(protection);
        delete space;
        Check(kernel->NumFreeFrame == originalFree, "free-frame count restored");
        for (int i = 0; i < NumPhysPages; i++) {
            Check(kernel->frameTable[i] == reserved[i], "reservation bitmap restored");
            if (reserved[i]) for (int j = 0; j < PageSize; j++)
                Check((unsigned char)kernel->machine->mainMemory[i*PageSize+j] == 0xa5,
                      "neighbor frame overwritten");
        }
    }
    delete [] image;
    puts("PASS: load and reclaim");
    return 0;
}
