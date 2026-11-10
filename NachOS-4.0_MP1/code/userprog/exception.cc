// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "ksyscall.h"
#include "main.h"
#include "syscall.h"
#include <cstring>
// User pointers refer to the active page table, never directly to mainMemory.
// Validate the entire requested span before file I/O so a bad later page cannot
// consume file bytes or partially overwrite a user buffer.
static bool UserRange(unsigned int address, int size, bool writing)
{
    if (size < 0 || size > MemorySize) return FALSE;
    if (size == 0) return TRUE;
    Machine *machine = kernel->machine;
    if (machine->pageTable == NULL || machine->pageTableSize == 0 ||
        machine->pageTableSize > NumPhysPages) return FALSE;
    unsigned int limit = machine->pageTableSize * PageSize;
    if (address >= limit || (unsigned int)size > limit - address) return FALSE;
    unsigned int last = (address + size - 1) / PageSize;
    for (unsigned int page = address / PageSize; page <= last; page++) {
        TranslationEntry &entry = machine->pageTable[page];
        if (!entry.valid || entry.physicalPage < 0 || entry.physicalPage >= NumPhysPages ||
            (writing && entry.readOnly)) return FALSE;
    }
    return TRUE;
}

static bool CopyUser(unsigned int address, char *buffer, int size, bool toUser)
{
    if (!UserRange(address, size, toUser)) return FALSE;
    int copied = 0;
    while (copied < size) {
        unsigned int current = address + copied;
        TranslationEntry &entry = kernel->machine->pageTable[current / PageSize];
        unsigned int offset = current % PageSize;
        int chunk = PageSize - offset;
        if (chunk > size - copied) chunk = size - copied;
        char *physical = &kernel->machine->mainMemory[entry.physicalPage * PageSize + offset];
        if (toUser) {
            memcpy(physical, buffer + copied, chunk);
            entry.dirty = TRUE;
        } else {
            memcpy(buffer + copied, physical, chunk);
        }
        entry.use = TRUE;
        copied += chunk;
    }
    return TRUE;
}

const int MaxUserString = 256; // Includes the terminating NUL.
static bool CopyUserString(unsigned int address, char *buffer)
{
    for (int i = 0; i < MaxUserString; i++) {
        if (!CopyUser(address + i, buffer + i, 1, FALSE)) return FALSE;
        if (buffer[i] == '\0') return TRUE;
    }
    return FALSE;
}

static int UserFileRead(unsigned int address, int size, int id)
{
    if (!UserRange(address, size, TRUE)) return -1;
    char *buffer = new char[size > 0 ? size : 1];
    int result = SysRead(buffer, size, id);
    if (result > size || (result > 0 && !CopyUser(address, buffer, result, TRUE))) result = -1;
    delete [] buffer;
    return result;
}

static int UserFileWrite(unsigned int address, int size, int id)
{
    if (!UserRange(address, size, FALSE)) return -1;
    char *buffer = new char[size > 0 ? size : 1];
    int result = CopyUser(address, buffer, size, FALSE) ? SysWrite(buffer, size, id) : -1;
    delete [] buffer;
    return result;
}


//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions
//	is in machine.h.
//----------------------------------------------------------------------
void ExceptionHandler(ExceptionType which) {
  char ch;
  int val;
  int type = kernel->machine->ReadRegister(2);
  int status, exit, id, size;
  DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");
  DEBUG(dbgTraCode, "In ExceptionHandler(), Received Exception " << which << " type: " << type << ", " << kernel->stats->totalTicks);
  switch (which) {
    case SyscallException:
      switch (type) {
        case SC_Halt:
          DEBUG(dbgSys, "Shutdown, initiated by user program.\n");
          SysHalt();
          cout << "in exception\n";
          ASSERTNOTREACHED();
          break;
        case SC_PrintInt:
          DEBUG(dbgSys, "Print Int\n");
          val = kernel->machine->ReadRegister(4);
          DEBUG(dbgTraCode, "In ExceptionHandler(), into SysPrintInt, " << kernel->stats->totalTicks);
          SysPrintInt(val);
          DEBUG(dbgTraCode, "In ExceptionHandler(), return from SysPrintInt, " << kernel->stats->totalTicks);
          // Set Program Counter
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
          return;
          ASSERTNOTREACHED();
          break;
        case SC_MSG:
          DEBUG(dbgSys, "Message received.\n");
          val = kernel->machine->ReadRegister(4);
          {
            char msg[MaxUserString];
            if (!CopyUserString((unsigned int)val, msg)) {
              kernel->machine->WriteRegister(2, -1);
              kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
              kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
              kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
              return;
            }
            cout << msg << endl;
          }
          SysHalt();
          ASSERTNOTREACHED();
          break;
        case SC_Create:
          val = kernel->machine->ReadRegister(4);
          {
            char filename[MaxUserString];
            status = CopyUserString((unsigned int)val, filename) ? SysCreate(filename) : 0;
            kernel->machine->WriteRegister(2, (int)status);
          }
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
          return;
          ASSERTNOTREACHED();
          break;
        // TODO (Open): Add SC_Open case to let the kernel able to handle this system call. You can imitate SC_Create case.
        case SC_Open:
          val = kernel->machine->ReadRegister(4);
          {
            DEBUG(dbgSys, "Open "<<kernel->machine->ReadRegister(4)<<"+"<<kernel->machine->ReadRegister(5)<< "\n");
            char filename[MaxUserString];
            status = CopyUserString((unsigned int)val, filename) ? SysOpen(filename) : -1;
            kernel->machine->WriteRegister(2, (int)status);
          }
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
          return;
          ASSERTNOTREACHED();
          break;
        // TODO (Write): Add SC_Write case to let the kernel able to handle this system call. You can imitate SC_Add case.
        case SC_Write:
          val = kernel->machine->ReadRegister(4);
          {
            DEBUG(dbgSys, "Write "<<kernel->machine->ReadRegister(4)<<"+"<<kernel->machine->ReadRegister(5)<< "\n");
            size = kernel->machine->ReadRegister(5);
            id = kernel->machine->ReadRegister(6);

            status = UserFileWrite((unsigned int)val, size, id);
            kernel->machine->WriteRegister(2, (int) status);
          }
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
          return;
          ASSERTNOTREACHED();
          break;
        // TODO (Read): Add SC_Read case to let the kernel able to handle this system call. You can imitate SC_Add case.
        case SC_Read:
          val = kernel->machine->ReadRegister(4);
          {
            DEBUG(dbgSys, "Read "<<kernel->machine->ReadRegister(4)<<"+"<<kernel->machine->ReadRegister(5)<< "\n");
            size = kernel->machine->ReadRegister(5);
            id = kernel->machine->ReadRegister(6);

            status = UserFileRead((unsigned int)val, size, id);
            kernel->machine->WriteRegister(2, (int) status);
          }
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
          return;
          ASSERTNOTREACHED();
          break;
        // TODO (Close): Add SC_Close case to let the kernel able to handle this system call. You can imitate SC_Create case.
        case SC_Close:
          val = kernel->machine->ReadRegister(4);
          {
            //char *filename = &(kernel->machine->mainMemory[val]);
            // cout << filename << endl;
            status = SysClose(val);
            kernel->machine->WriteRegister(2, (int)status);
          }
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
          return;
          ASSERTNOTREACHED();
          break;
        case SC_Add:
          DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5) << "\n");
          /* Process SysAdd Systemcall*/
          int result;
          result = SysAdd(/* int op1 */ (int)kernel->machine->ReadRegister(4),
                          /* int op2 */ (int)kernel->machine->ReadRegister(5));
          DEBUG(dbgSys, "Add returning with " << result << "\n");
          /* Prepare Result */
          kernel->machine->WriteRegister(2, (int)result);
          /* Modify return point */
          {
            /* set previous programm counter (debugging only)*/
            kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

            /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
            kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

            /* set next programm counter for brach execution */
            kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
          }
          cout << "result is " << result << "\n";
          return;
          ASSERTNOTREACHED();
          break;
        case SC_Exit:
          DEBUG(dbgAddr, "Program exit\n");
          val = kernel->machine->ReadRegister(4);
          cout << "return value:" << val << endl;
          kernel->currentThread->Finish();
          break;
        default:
          cerr << "Unexpected system call " << type << "\n";
          break;
      }
      break;
    default:
      cerr << "Unexpected user mode exception " << (int)which << "\n";
      break;
  }
  ASSERTNOTREACHED();
}
