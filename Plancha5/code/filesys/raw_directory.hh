/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2017 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_FILESYS_RAWDIRECTORY__HH
#define NACHOS_FILESYS_RAWDIRECTORY__HH
#include "directory_entry.hh"

class DirectoryEntry;

struct RawDirectory {
    int dirFather; //Sector donde se almacena el directorio padre
    unsigned tableSize;  ///< Number of directory entries.
    DirectoryEntry table[10];  ///< Table of pairs:
                            ///< *<file name, file header location>*.
};


#endif
