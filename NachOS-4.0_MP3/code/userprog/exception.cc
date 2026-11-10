// exception.cc -- implemented syscall memory boundary and dispatch.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include <iostream>
#include <memory>
#include <new>
#include <cstring>
#include "copyright.h"
#include "main.h"
#include "syscall.h"
#include "ksyscall.h"

namespace {
const unsigned int MaxUserString = 256; // Bytes including the terminating NUL.

bool UserRange(unsigned int address, int size, bool writing)
{
    if (size < 0 || size > MemorySize) return false;
    if (size == 0) return true; // Empty spans never dereference the address.
    Machine *machine = kernel->machine;
    if (kernel->currentThread == NULL || kernel->currentThread->space == NULL ||
        machine->pageTable == NULL || machine->pageTableSize == 0 ||
        machine->pageTableSize > NumPhysPages) return false;
    unsigned int limit = machine->pageTableSize * PageSize;
    if (address >= limit || static_cast<unsigned int>(size) > limit - address) return false;
    unsigned int last = (address + size - 1) / PageSize;
    for (unsigned int page = address / PageSize; page <= last; ++page) {
        const TranslationEntry &entry = machine->pageTable[page];
        if (!entry.valid || entry.physicalPage < 0 || entry.physicalPage >= NumPhysPages ||
            (writing && entry.readOnly)) return false;
    }
    return true;
}

bool CopyUser(unsigned int address, char *buffer, int size, bool toUser)
{
    // Validate every page first: a bad tail must not modify earlier bytes.
    if (!UserRange(address, size, toUser)) return false;
    int copied = 0;
    while (copied < size) {
        unsigned int current = address + copied;
        TranslationEntry &entry = kernel->machine->pageTable[current / PageSize];
        unsigned int offset = current % PageSize;
        int chunk = PageSize - offset;
        if (chunk > size - copied) chunk = size - copied;
        char *physical = kernel->machine->mainMemory + entry.physicalPage * PageSize + offset;
        if (toUser) {
            memcpy(physical, buffer + copied, chunk);
            entry.dirty = true;
        } else {
            memcpy(buffer + copied, physical, chunk);
        }
        entry.use = true;
        copied += chunk;
    }
    return true;
}

bool CopyUserString(unsigned int address, char *buffer)
{
    for (unsigned int i = 0; i < MaxUserString; ++i) {
        if (!CopyUser(address + i, buffer + i, 1, false)) return false;
        if (buffer[i] == '\0') return true;
    }
    return false;
}

int ReadUserFile(unsigned int address, int size, int id)
{
    // Check the full request even if EOF might return fewer bytes. Otherwise
    // an invalid tail could consume file input before it can be copied out.
    if (!UserRange(address, size, true)) return -1;
    std::unique_ptr<char[]> buffer(new (std::nothrow) char[size > 0 ? size : 1]);
    if (!buffer) return -1;
    int count = SysRead(buffer.get(), size, id);
    if (count > size || (count > 0 && !CopyUser(address, buffer.get(), count, true))) return -1;
    return count;
}

int WriteUserFile(unsigned int address, int size, int id)
{
    if (!UserRange(address, size, false)) return -1;
    std::unique_ptr<char[]> buffer(new (std::nothrow) char[size > 0 ? size : 1]);
    if (!buffer || !CopyUser(address, buffer.get(), size, false)) return -1;
    int count = SysWrite(buffer.get(), size, id);
    return count > size ? -1 : count;
}

void AdvancePC()
{
    Machine *machine = kernel->machine;
    unsigned int next = static_cast<unsigned int>(machine->ReadRegister(NextPCReg));
    machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
    machine->WriteRegister(PCReg, static_cast<int>(next));
    machine->WriteRegister(NextPCReg, static_cast<int>(next + 4U));
}
}

void ExceptionHandler(ExceptionType which)
{
    Machine *machine = kernel->machine;
    int type = machine->ReadRegister(2);
    DEBUG(dbgSys, "Received Exception " << which << " type: " << type);
    DEBUG(dbgTraCode, "In ExceptionHandler(), Received Exception " << which
          << " type: " << type << ", " << kernel->stats->totalTicks);
    if (which != SyscallException) {
        cerr << "Unexpected user mode exception " << static_cast<int>(which) << "\n";
        ASSERTNOTREACHED();
    }
    int first = machine->ReadRegister(4);
    int result;
    switch (type) {
      case SC_Halt:
        SysHalt();
        ASSERTNOTREACHED();
        return;
      case SC_Exit:
        cout << "return value:" << first << endl;
        kernel->currentThread->Finish();
        ASSERTNOTREACHED();
        return;
      case SC_PrintInt:
        SysPrintInt(first);
        AdvancePC();
        return;
      case SC_Add:
        result = SysAdd(first, machine->ReadRegister(5));
        cout << "result is " << result << "\n";
        break;
      case SC_MSG: {
        char message[MaxUserString];
        if (!CopyUserString(static_cast<unsigned int>(first), message)) {
            result = -1;
            break;
        }
        cout << message << endl;
        SysHalt(); // Preserve the original valid-message whole-machine behavior.
        ASSERTNOTREACHED();
        return;
      }
      case SC_Create:
      case SC_Open: {
        char name[MaxUserString];
        if (!CopyUserString(static_cast<unsigned int>(first), name) || name[0] == '\0') result = -1;
        else result = type == SC_Create ? SysCreate(name) : SysOpen(name);
        break;
      }
      case SC_Read:
        result = ReadUserFile(static_cast<unsigned int>(first), machine->ReadRegister(5), machine->ReadRegister(6));
        break;
      case SC_Write:
        result = WriteUserFile(static_cast<unsigned int>(first), machine->ReadRegister(5), machine->ReadRegister(6));
        break;
      case SC_Close:
        result = SysClose(first);
        break;
      default:
        cerr << "Unexpected system call " << type << "\n";
        ASSERTNOTREACHED();
        return;
    }
    machine->WriteRegister(2, result);
    AdvancePC();
}
