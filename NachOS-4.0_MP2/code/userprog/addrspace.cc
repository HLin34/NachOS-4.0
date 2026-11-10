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

static void 
SwapHeader (NoffHeader *noffH)
{
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
#ifdef RDATA
    noffH->readonlyData.size = WordToHost(noffH->readonlyData.size);
    noffH->readonlyData.virtualAddr = 
           WordToHost(noffH->readonlyData.virtualAddr);
    noffH->readonlyData.inFileAddr = 
           WordToHost(noffH->readonlyData.inFileAddr);
#endif 
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);

#ifdef RDATA
    DEBUG(dbgAddr, "code = " << noffH->code.size <<  
                   " readonly = " << noffH->readonlyData.size <<
                   " init = " << noffH->initData.size <<
                   " uninit = " << noffH->uninitData.size << "\n");
#endif
}

// Read each chunk only up to its virtual page boundary: consecutive virtual
// pages may occupy nonconsecutive physical frames.
static bool LoadSegment(AddrSpace *space, OpenFile *executable,
                        const Segment &segment)
{
    int consumed = 0;
    while (consumed < segment.size) {
        unsigned int virtualAddress = segment.virtualAddr + consumed;
        unsigned int physicalAddress;
        if (space->Translate(virtualAddress, &physicalAddress, FALSE) != NoException)
            return FALSE;
        int chunk = PageSize - (virtualAddress % PageSize);
        if (chunk > segment.size - consumed) chunk = segment.size - consumed;
        if (executable->ReadAt(&kernel->machine->mainMemory[physicalAddress],
                               chunk, segment.inFileAddr + consumed) != chunk)
            return FALSE;
        consumed += chunk;
    }
    return TRUE;
}

static void ReleaseFrames(TranslationEntry *table, unsigned int pages)
{
    for (unsigned int i = 0; i < pages; i++) {
        kernel->frameTable[table[i].physicalPage] = FALSE;
        kernel->NumFreeFrame++;
    }
    delete [] table;
}

static bool MemoryLoadFailure(OpenFile *executable)
{
    // Return to the loader's caller rather than terminating inside the handler:
    // ForkExecute/ThreadRoot will finish only the failed program.
    delete executable;
    ExceptionHandler(MemoryLimitException);
    return FALSE;
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Start empty; Load allocates and initializes the program page table.
//----------------------------------------------------------------------

AddrSpace::AddrSpace()
{
    pageTable = NULL;
    numPages = 0;

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    ReleaseFrames(pageTable, numPages);
}


//----------------------------------------------------------------------
// AddrSpace::Load
// 	Load a user program into memory from a file.
//
//	Allocates the page table and loads the object code in NOFF format.
//
//	"fileName" is the file containing the object code to load into memory
//----------------------------------------------------------------------

bool 
AddrSpace::Load(char *fileName) 
{
    OpenFile *executable = kernel->fileSystem->Open(fileName);
    NoffHeader noffH;
    unsigned int size;

    if (executable == NULL) {
	cerr << "Unable to open file " << fileName << "\n";
	return FALSE;
    }

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

    Segment segments[] = {
        noffH.code, noffH.initData, noffH.uninitData,
#ifdef RDATA
        noffH.readonlyData,
#endif
    };
    unsigned int imageEnd = 0;
    for (unsigned int i = 0; i < sizeof(segments)/sizeof(segments[0]); i++) {
        const Segment &segment = segments[i];
        if (segment.size < 0) {
            delete executable;
            return FALSE;
        }
        // The legacy converter leaves metadata of absent segments unspecified.
        if (segment.size == 0) continue;
        // BSS has no file payload, so its file offset is also irrelevant.
        if (segment.virtualAddr < 0 || (i != 2 && segment.inFileAddr < 0)) {
            delete executable;
            return FALSE;
        }
        // Check before addition so malformed headers cannot wrap the size.
        if ((unsigned int)segment.virtualAddr > MemorySize ||
            (unsigned int)segment.size > MemorySize - (unsigned int)segment.virtualAddr) {
            return MemoryLoadFailure(executable);
        }
        unsigned int end = segment.virtualAddr + segment.size;
        if (end > imageEnd) imageEnd = end;
    }
    size = imageEnd + UserStackSize;
    unsigned int requiredPages = divRoundUp(size, PageSize);
    if (kernel->NumFreeFrame < 0 || requiredPages > (unsigned int)kernel->NumFreeFrame)
        return MemoryLoadFailure(executable);

    // numPages records committed mappings, including on an interrupted allocation.
    numPages = 0;
    pageTable = new TranslationEntry[requiredPages];
    int frame = 0;
    for (unsigned int page = 0; page < requiredPages; page++) {
        while (frame < NumPhysPages && kernel->frameTable[frame]) frame++;
        if (frame == NumPhysPages) {
            ReleaseFrames(pageTable, numPages);
            pageTable = NULL;
            numPages = 0;
            return MemoryLoadFailure(executable);
        }
        bzero(&kernel->machine->mainMemory[frame * PageSize], PageSize);
        pageTable[page].virtualPage = page;
        pageTable[page].physicalPage = frame;
        pageTable[page].valid = TRUE;
        pageTable[page].use = FALSE;
        pageTable[page].dirty = FALSE;
        pageTable[page].readOnly = FALSE;
        kernel->frameTable[frame] = TRUE;
        kernel->NumFreeFrame--;
        numPages++;
        frame++;
    }

    bool loaded = LoadSegment(this, executable, noffH.code) &&
                  LoadSegment(this, executable, noffH.initData);
#ifdef RDATA
    loaded = loaded && LoadSegment(this, executable, noffH.readonlyData);
#endif
    if (!loaded) {
        ReleaseFrames(pageTable, numPages);
        pageTable = NULL;
        numPages = 0;
        delete executable;
        return FALSE;
    }
#ifdef RDATA
    // Protection is page-granular. A page shared with writable data/BSS/stack
    // must stay writable even if it also contains read-only bytes.
    if (noffH.readonlyData.size > 0) {
        unsigned int roStart = noffH.readonlyData.virtualAddr;
        unsigned int roEnd = roStart + noffH.readonlyData.size;
        unsigned int stackStart = numPages * PageSize - UserStackSize;
        for (unsigned int page = 0; page < numPages; page++) {
            unsigned int start = page * PageSize;
            unsigned int end = start + PageSize;
            bool writable = end > stackStart;
            Segment data[] = { noffH.initData, noffH.uninitData };
            for (unsigned int i = 0; i < 2; i++) {
                if (data[i].size > 0 && start < (unsigned int)(data[i].virtualAddr + data[i].size)
                    && end > (unsigned int)data[i].virtualAddr) writable = TRUE;
            }
            pageTable[page].readOnly = start < roEnd && end > roStart && !writable;
        }
    }
#endif

    delete executable;			// close file
    return TRUE;			// success
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
//MP2
void AddrSpace::RaiseException(ExceptionType which)
{
    kernel->interrupt->setStatus(SystemMode);
    ExceptionHandler(which);
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
    TranslationEntry *pte;
    int               pfn;
    unsigned int      vpn    = vaddr / PageSize;
    unsigned int      offset = vaddr % PageSize;

    if(vpn >= numPages) {
        return AddressErrorException;
    }

    pte = &pageTable[vpn];

    if(isReadWrite && pte->readOnly) {
        return ReadOnlyException;
    }

    pfn = pte->physicalPage;

    // if the pageFrame is too big, there is something really wrong!
    // An invalid translation was loaded into the page table or TLB.
    if (pfn >= NumPhysPages) {
        DEBUG(dbgAddr, "Illegal physical page " << pfn);
        return BusErrorException;
    }

    pte->use = TRUE;          // set the use, dirty bits

    if(isReadWrite)
        pte->dirty = TRUE;

    *paddr = pfn*PageSize + offset;

    ASSERT((*paddr < MemorySize));

    //cerr << " -- AddrSpace::Translate(): vaddr: " << vaddr <<
    //  ", paddr: " << *paddr << "\n";

    return NoException;
}




