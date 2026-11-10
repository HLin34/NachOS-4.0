// filesys.h 
//	Data structures to represent the Nachos file system.
//
//	A file system is a set of files stored on disk, organized
//	into directories.  Operations on the file system have to
//	do with "naming" -- creating, opening, and deleting files,
//	given a textual file name.  Operations on an individual
//	"open" file (read, write, close) are to be found in the OpenFile
//	class (openfile.h).
//
//	We define two separate implementations of the file system. 
//	The "STUB" version just re-defines the Nachos file system 
//	operations as operations on the native UNIX file system on the machine
//	running the Nachos simulation.
//
//	The other version is a "real" file system, built on top of 
//	a disk simulator.  The disk is simulated using the native UNIX 
//	file system (in a file named "DISK"). 
//
//	In the "real" implementation, there are two key data structures used 
//	in the file system.  There is a single "root" directory, listing
//	all of the files in the file system; unlike UNIX, the baseline
//	system does not provide a hierarchical directory structure.  
//	In addition, there is a bitmap for allocating
//	disk sectors.  Both the root directory and the bitmap are themselves
//	stored as files in the Nachos file system -- this causes an interesting
//	bootstrap problem when the simulated disk is initialized. 
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef FS_H
#define FS_H

#include "copyright.h"
#include "sysdep.h"
#include "openfile.h"
#include "debug.h" 		//just for test!!!

#ifdef FILESYS_STUB 		// Temporarily implement file system calls as 
				// calls to UNIX, until the real file system
				// implementation is available
typedef int OpenFileId;

class Thread;
class FileSystem {
  public:
    static const int MaxOpenFiles = 20; // Shared capacity; IDs remain 1..20.
    FileSystem() {
        for (int i = 0; i < MaxOpenFiles; ++i) {
            OpenFileTable[i] = NULL; owners[i] = NULL;
        }
    }
    ~FileSystem() {
        for (int i = 0; i < MaxOpenFiles; ++i) Release(i);
    }
    bool Create(char *name) {
        int fd = StubOpen(name, true);
        return fd >= 0 && StubClose(fd) == 0;
    }
    // Loader opens are scoped by AddrSpace and do not consume user slots.
    OpenFile* Open(char *name) {
        int fd = StubOpen(name, false);
        if (fd < 0) return NULL;
        try { return new OpenFile(fd); }
        catch (...) { StubClose(fd); return NULL; }
    }
    OpenFileId OpenAFile(char *name, Thread *owner = NULL) {
        for (int i = 0; i < MaxOpenFiles; ++i) {
            if (OpenFileTable[i] == NULL) {
                OpenFile *file = Open(name);
                if (file == NULL) return -1;
                owners[i] = owner; OpenFileTable[i] = file;
                return i + 1;
            }
        }
        return -1; // Check capacity before opening a host descriptor.
    }
    int WriteFile(char *buffer, int size, OpenFileId id, Thread *owner = NULL) {
        if (!Accessible(id, owner) || size < 0) return -1;
        return OpenFileTable[id-1]->Write(buffer, size);
    }
    int ReadFile(char *buffer, int size, OpenFileId id, Thread *owner = NULL) {
        if (!Accessible(id, owner) || size < 0) return -1;
        return OpenFileTable[id-1]->Read(buffer, size);
    }
    int CloseFile(OpenFileId id, Thread *owner = NULL) {
        if (!Accessible(id, owner)) return -1;
        return Release(id - 1) == 0 ? 1 : -1;
    }
    void CloseAllFor(Thread *owner) {
        if (owner == NULL) return;
        for (int i = 0; i < MaxOpenFiles; ++i)
            if (OpenFileTable[i] != NULL && owners[i] == owner) Release(i);
    }
    bool Remove(char *name) { return Unlink(name) == 0; }
    OpenFile *OpenFileTable[MaxOpenFiles]; // Kept public for legacy diagnostics.
  private:
    Thread *owners[MaxOpenFiles];
    bool Accessible(OpenFileId id, Thread *owner) {
        return id >= 1 && id <= MaxOpenFiles && OpenFileTable[id-1] != NULL &&
               owners[id-1] == owner;
    }
    int Release(int slot) {
        OpenFile *file = OpenFileTable[slot];
        OpenFileTable[slot] = NULL; owners[slot] = NULL;
        if (file == NULL) return 0;
        int result = file->CloseHandle();
        delete file; // CloseHandle consumed the fd; destructor never retries.
        return result;
    }
};

#else // FILESYS
class FileSystem {
  public:
    FileSystem(bool format);		// Initialize the file system.
					// Must be called *after* "synchDisk" 
					// has been initialized.
    					// If "format", there is nothing on
					// the disk, so initialize the directory
    					// and the bitmap of free blocks.

    bool Create(char *name, int initialSize);  	
					// Create a file (UNIX creat)

    OpenFile* Open(char *name); 	// Open a file (UNIX open)

    bool Remove(char *name);  		// Delete a file (UNIX unlink)

    void List();			// List all the files in the file system

    void Print();			// List all the files and their contents

  private:
   OpenFile* freeMapFile;		// Bit map of free disk blocks,
					// represented as a file
   OpenFile* directoryFile;		// "Root" directory -- list of 
					// file names, represented as a file
};

#endif // FILESYS

#endif // FS_H
