// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -n -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you are using the "stub" file system, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include <iostream>
#include <memory>
#include "main.h"
#include "addrspace.h"
#include "machine.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

// NOFF headers may carry either byte order; payload bytes are unchanged.
static unsigned int SwapWord(unsigned int word)
{
    return ((word & 0xffU) << 24) | ((word & 0xff00U) << 8) |
           ((word >> 8) & 0xff00U) | ((word >> 24) & 0xffU);
}

static void 
SwapHeader (NoffHeader *noffH)
{
    noffH->noffMagic = SwapWord(noffH->noffMagic);
    noffH->code.size = SwapWord(noffH->code.size);
    noffH->code.virtualAddr = SwapWord(noffH->code.virtualAddr);
    noffH->code.inFileAddr = SwapWord(noffH->code.inFileAddr);
#ifdef RDATA
    noffH->readonlyData.size = SwapWord(noffH->readonlyData.size);
    noffH->readonlyData.virtualAddr = 
           SwapWord(noffH->readonlyData.virtualAddr);
    noffH->readonlyData.inFileAddr = 
           SwapWord(noffH->readonlyData.inFileAddr);
#endif 
    noffH->initData.size = SwapWord(noffH->initData.size);
    noffH->initData.virtualAddr = SwapWord(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = SwapWord(noffH->initData.inFileAddr);
    noffH->uninitData.size = SwapWord(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = SwapWord(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = SwapWord(noffH->uninitData.inFileAddr);

#ifdef RDATA
    DEBUG(dbgAddr, "code = " << noffH->code.size <<  
                   " readonly = " << noffH->readonlyData.size <<
                   " init = " << noffH->initData.size <<
                   " uninit = " << noffH->uninitData.size << "\n");
#endif
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//----------------------------------------------------------------------

AddrSpace::AddrSpace()
{
    numPages = 0;
    pageTable = new TranslationEntry[NumPhysPages];
    for (int i = 0; i < NumPhysPages; i++) {
        pageTable[i].virtualPage = i;	// for now, virt page # = phys page #
// *************** MP2 *************** //
        pageTable[i].physicalPage = -1;
        pageTable[i].valid = FALSE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;  
    }
    // zero out the entire address space
    // bzero(kernel->machine->mainMemory, MemorySize);
// *************** MP2 *************** //
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    ReleaseFrames();
    delete [] pageTable;
}

namespace {
class InterruptGuard {
  public:
    InterruptGuard() : old(kernel->interrupt->SetLevel(IntOff)) {}
    ~InterruptGuard() { kernel->interrupt->SetLevel(old); }
  private:
    IntStatus old;
    InterruptGuard(const InterruptGuard&);
    InterruptGuard& operator=(const InterruptGuard&);
};
bool InvalidImage(const char *name, const char *reason) {
    cerr << "Invalid executable " << name << ": " << reason << "\n";
    return false;
}
bool NoLoadMemory(const char *name) {
    cerr << "Insufficient memory to load " << name << "\n";
    return false;
}
bool Intersects(unsigned long long begin, unsigned long long end, const Segment &segment) {
    return segment.size > 0 && begin < static_cast<unsigned long long>(segment.virtualAddr) + segment.size &&
           end > static_cast<unsigned long long>(segment.virtualAddr);
}
}

void AddrSpace::ReleaseFrames()
{
    InterruptGuard guard;
    if (kernel->machine->pageTable == pageTable) {
        kernel->machine->pageTable = NULL;
        kernel->machine->pageTableSize = 0;
    }
    for (int i = 0; i < NumPhysPages; ++i) {
        TranslationEntry &entry = pageTable[i];
        if (entry.physicalPage != -1) {
            ASSERT(entry.physicalPage >= 0 && entry.physicalPage < NumPhysPages);
            ASSERT(kernel->usedPhysPage[entry.physicalPage]);
            kernel->usedPhysPage[entry.physicalPage] = false;
            ++kernel->numAvailPhysPage;
        }
        entry.physicalPage = -1;
        entry.valid = entry.use = entry.dirty = entry.readOnly = false;
    }
    numPages = 0;
}

//----------------------------------------------------------------------
// AddrSpace::Load
// 	Load a user program into memory from a file.
//
//	Assumes that the page table has been initialized, and that
//	the object code file is in NOFF format.
//
//	"fileName" is the file containing the object code to load into memory
//----------------------------------------------------------------------

bool
AddrSpace::Load(char *fileName)
{
    if (numPages != 0) return InvalidImage(fileName, "address space already loaded");
    std::unique_ptr<OpenFile> executable(kernel->fileSystem->Open(fileName));
    if (!executable) {
        cerr << "Unable to open file " << fileName << "\n";
        return false;
    }
    NoffHeader header = {};
    int fileLength = executable->Length();
    if (fileLength < static_cast<int>(sizeof header) ||
        executable->ReadAt(reinterpret_cast<char*>(&header), sizeof header, 0) != sizeof header)
        return InvalidImage(fileName, "short header");
    if (header.noffMagic != NOFFMAGIC && SwapWord(header.noffMagic) == NOFFMAGIC)
        SwapHeader(&header);
    if (header.noffMagic != NOFFMAGIC) return InvalidImage(fileName, "bad magic");

    Segment regions[] = {header.code, header.initData,
#ifdef RDATA
        header.readonlyData,
#endif
        header.uninitData};
    const int regionCount = sizeof regions / sizeof regions[0];
    unsigned long long imageEnd = 0;
    for (int i = 0; i < regionCount; ++i) {
        const Segment &region = regions[i];
        if (region.size < 0 || (region.size > 0 && region.virtualAddr < 0))
            return InvalidImage(fileName, "negative segment range");
        if (region.size == 0) continue;
        unsigned long long end = static_cast<unsigned long long>(region.virtualAddr) + region.size;
        if (end > imageEnd) imageEnd = end;
        if (i != regionCount - 1 &&
            (region.inFileAddr < static_cast<int>(sizeof header) ||
             static_cast<unsigned long long>(region.inFileAddr) + region.size > static_cast<unsigned long long>(fileLength)))
            return InvalidImage(fileName, "payload outside file");
        for (int j = 0; j < i; ++j)
            if (Intersects(region.virtualAddr, end, regions[j]))
                return InvalidImage(fileName, "overlapping virtual segments");
    }
    // Wide arithmetic avoids signed overflow and accounts for sparse virtual gaps.
    unsigned long long total = imageEnd + UserStackSize;
    if (total > MemorySize) return NoLoadMemory(fileName);
    unsigned int needed = (total + PageSize - 1) / PageSize;
    {
        InterruptGuard guard;
        int actualFree = 0;
        for (int frame = 0; frame < NumPhysPages; ++frame)
            if (!kernel->usedPhysPage[frame]) ++actualFree;
        if (actualFree != kernel->numAvailPhysPage)
            return InvalidImage(fileName, "frame accounting mismatch");
        if (needed > static_cast<unsigned int>(actualFree)) return NoLoadMemory(fileName);
        numPages = needed;
        int frame = 0;
        for (unsigned int page = 0; page < numPages; ++page) {
            while (frame < NumPhysPages && kernel->usedPhysPage[frame]) ++frame;
            if (frame == NumPhysPages) { ReleaseFrames(); return NoLoadMemory(fileName); }
            pageTable[page].physicalPage = frame;
            pageTable[page].valid = true;
            kernel->usedPhysPage[frame] = true;
            --kernel->numAvailPhysPage;
            bzero(kernel->machine->mainMemory + frame * PageSize, PageSize);
        }
    }
    for (int i = 0; i < regionCount - 1; ++i) {
        const Segment &region = regions[i];
        if (!LoadData(region.virtualAddr, region.inFileAddr, region.size, executable.get(), 1)) {
            ReleaseFrames();
            return InvalidImage(fileName, "short payload or invalid mapping");
        }
    }
    for (unsigned int page = 0; page < numPages; ++page) {
        pageTable[page].use = pageTable[page].dirty = false;
#ifdef RDATA
        unsigned long long begin = page * PageSize, end = begin + PageSize;
        Segment stack = {static_cast<int>(numPages * PageSize - UserStackSize), 0, UserStackSize};
        // A shared page must remain writable for code/data/BSS/stack users.
        pageTable[page].readOnly = Intersects(begin, end, header.readonlyData) &&
            !Intersects(begin, end, header.code) && !Intersects(begin, end, header.initData) &&
            !Intersects(begin, end, header.uninitData) && !Intersects(begin, end, stack);
#endif
    }
    return true;
}

bool
AddrSpace::LoadData(int virtualAddress, int fileOffset, int size, OpenFile *file, int isReadWrite)
{
    (void)isReadWrite; // Protection is applied after all overlapping pages are known.
    if (size < 0 || (size > 0 && (virtualAddress < 0 || fileOffset < 0))) return false;
    if (size == 0) return true;
    if (static_cast<unsigned long long>(virtualAddress) + size > numPages * PageSize) return false;
    int remaining = size;
    while (remaining > 0) {
        unsigned int physical;
        if (Translate(virtualAddress, &physical, 0) != NoException) return false;
        int capacity = PageSize - virtualAddress % PageSize;
        int chunk = remaining < capacity ? remaining : capacity;
        if (file->ReadAt(kernel->machine->mainMemory + physical, chunk, fileOffset) != chunk) return false;
        virtualAddress += chunk;
        fileOffset += chunk;
        remaining -= chunk;
    }
    return true;
}

//----------------------------------------------------------------------
// AddrSpace::Execute
// 	Run a user program using the current thread
//
//      The program is assumed to have already been loaded into
//      the address space
//
//----------------------------------------------------------------------

void 
AddrSpace::Execute(char* fileName) 
{

    kernel->currentThread->space = this;

    this->InitRegisters();		// set the initial register values
    this->RestoreState();		// load page table register

    kernel->machine->Run();		// jump to the user progam

    ASSERTNOTREACHED();			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}


//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    Machine *machine = kernel->machine;
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start", which
    //  is assumed to be virtual address zero
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    // Since instructions occupy four bytes each, the next instruction
    // after start will be at virtual address four.
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG(dbgAddr, "Initializing stack pointer: " << numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, don't need to save anything!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    kernel->machine->pageTable = pageTable;
    kernel->machine->pageTableSize = numPages;
}


//----------------------------------------------------------------------
// AddrSpace::Translate
//  Translate the virtual address in _vaddr_ to a physical address
//  and store the physical address in _paddr_.
//  The flag _isReadWrite_ is false (0) for read-only access; true (1)
//  for read-write access.
//  Return any exceptions caused by the address translation.
//----------------------------------------------------------------------
ExceptionType
AddrSpace::Translate(unsigned int vaddr, unsigned int *paddr, int isReadWrite)
{
    unsigned int vpn = vaddr / PageSize;
    if (paddr == NULL || vpn >= numPages || vpn >= NumPhysPages) return AddressErrorException;
    TranslationEntry &entry = pageTable[vpn];
    if (!entry.valid) return PageFaultException;
    int frame = entry.physicalPage;
    if (frame < 0 || frame >= NumPhysPages) return BusErrorException;
    if (isReadWrite && entry.readOnly) return ReadOnlyException;
    entry.use = true;
    if (isReadWrite) entry.dirty = true;
    *paddr = frame * PageSize + vaddr % PageSize;
    return NoException;
}
