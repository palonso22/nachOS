/// Routines to manage the overall operation of the file system.  Implements
/// routines to map from textual file names to files.
///
/// Each file in the file system has:
/// * a file header, stored in a sector on disk (the size of the file header
///   data structure is arranged to be precisely the size of 1 disk sector);
/// * a number of data blocks;
/// * an entry in the file system directory.
///
/// The file system consists of several data structures:
/// * A bitmap of free disk sectors (cf. `bitmap.h`).
/// * A directory of file names and file headers.
///
/// Both the bitmap and the directory are represented as normal files.  Their
/// file headers are located in specific sectors (sector 0 and sector 1), so
/// that the file system can find them on bootup.
///
/// The file system assumes that the bitmap and directory files are kept
/// “open” continuously while Nachos is running.
///
/// For those operations (such as `Create`, `Remove`) that modify the
/// directory and/or bitmap, if the operation succeeds, the changes are
/// written immediately back to disk (the two files are kept open during all
/// this time).  If the operation fails, and we have modified part of the
/// directory and/or bitmap, we simply discard the changed version, without
/// writing it back to disk.
///
/// Our implementation at this point has the following restrictions:
///
/// * there is no synchronization for concurrent accesses;
/// * files have a fixed size, set when the file is created;
/// * files cannot be bigger than about 3KB in size;
/// * there is no hierarchical directory structure, and only a limited number
///   of files can be added to the system;
/// * there is no attempt to make the system robust to failures (if Nachos
///   exits in the middle of an operation that modifies the file system, it
///   may corrupt the disk).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2018 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_system.hh"
#include "directory.hh"
#include "directory_entry.hh"
#include "file_header.hh"
#include "lib/bitmap.hh"
#include "machine/disk.hh"
#include "lib/table2.hh"
#include "lockwr.hh"
extern TableT<LockWR>* openFileTable;

// Sectors containing the file headers for the bitmap of free sectors, and
/// the directory of files.  These file headers are placed in well-known
/// sectors, so that they can be located on boot-up.
static const unsigned FREE_MAP_SECTOR = 0;
static const unsigned DIRECTORY_SECTOR = 1;

/// Initial file sizes for the bitmap and directory; until the file system
/// supports extensible files, the directory size sets the maximum number of
/// files that can be loaded onto the disk.
static const unsigned FREE_MAP_FILE_SIZE = NUM_SECTORS / BITS_IN_BYTE;
/// Initialize the file system.  If `format == true`, the disk has nothing on
/// it, and we need to initialize the disk to contain an empty directory, and
/// a bitmap of free sectors (with almost but not all of the sectors marked
/// as free).
///
/// If `format == false`, we just have to open the files representing the
/// bitmap and the directory.
///
/// * `format` -- should we initialize the disk?


//Rutina para crear un directorio					    
OpenFile*
FileSystem::CreateDirectory(unsigned sector,Bitmap* freeMap,int df){       

        Directory  *directory = new Directory(NUM_DIR_ENTRIES,sector,df);	
        FileHeader *dirHeader = new FileHeader;	
        ASSERT(dirHeader->Allocate(freeMap, DIRECTORY_FILE_SIZE));
        dirHeader->WriteBack(sector);
        OpenFile* dirFile = new OpenFile(sector);
        directory->WriteBack(dirFile);
        delete directory;
        delete dirHeader;
	return dirFile;
 
}
FileSystem::FileSystem(bool format)
{
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        Bitmap     *freeMap   = new Bitmap(NUM_SECTORS);
        FileHeader *mapHeader = new FileHeader;
        DEBUG('f', "Formatting the file system.\n");
     
        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FREE_MAP_SECTOR);
        freeMap->Mark(DIRECTORY_SECTOR);	

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapHeader->Allocate(freeMap, FREE_MAP_FILE_SIZE));


        // Flush the bitmap and directory `FileHeader`s back to disk.
        // We need to do this before we can `Open` the file, since open reads
        // the file header off of disk (and currently the disk has garbage on
        // it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapHeader->WriteBack(FREE_MAP_SECTOR);




	

        // OK to open the bitmap and directory files now.
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile   = new OpenFile(FREE_MAP_SECTOR);
	//Crear directorio root
	//No tiene directorio padre y esto lo representamos con -1	
        directoryFile = CreateDirectory(DIRECTORY_SECTOR,freeMap,-1);




        // Once we have the files “open”, we can write the initial version of
        // each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);     // flush changes to disk


        if (debug.IsEnabled('f')) {
            freeMap->Print();
            delete freeMap;
            delete mapHeader;
       }
    } else {
        // If we are not formatting the disk, just open the files
        // representing the bitmap and directory; these are left open while
        // Nachos is running.
        freeMapFile   = new OpenFile(FREE_MAP_SECTOR);
        directoryFile = new OpenFile(DIRECTORY_SECTOR);
    }
}

FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryFile;
}

/// Create a file in the Nachos file system (similar to UNIX `create`).
/// Since we cannot increase the size of files dynamically, we have to give
/// Create the initial size of the file.
///
/// The steps to create a file are:
/// 1. Make sure the file does not already exist.
/// 2. Allocate a sector for the file header.
/// 3. Allocate space on disk for the data blocks for the file.
/// 4. Add the name to the directory.
/// 5. Store the new file header on disk.
/// 6. Flush the changes to the bitmap and the directory back to disk.
///
/// Return true if everything goes ok, otherwise, return false.
///
/// Create fails if:
/// * file is already in directory;
/// * no free space for file header;
/// * no free entry for file in directory;
/// * no free space for data blocks for the file.
//
/// Note that this implementation assumes there is no concurrent access to
/// the file system!
///
/// * `name` is the name of file to be created.
/// * `initialSize` is the size of file to be created.
bool
FileSystem::Create(const char *name)
{
    ASSERT(name != nullptr);

    Directory  *directory;
    Bitmap     *freeMap;
    FileHeader *header;
    int         sector;
    bool        success;

    DEBUG('f', "Creating file %s\n", name);

    directory = new Directory(NUM_DIR_ENTRIES,DIRECTORY_SECTOR,-1);

    //Directorio root
    directory->FetchFrom(directoryFile);

    //Chequear si el archivo existe
    if (directory->Find(name,false) != -1){
        DEBUG('f',"Ya existe un archivo con el nombre %s\n",name);
        success = false;  // File is already in directory.
    
    }

    else {

        freeMap = new Bitmap(NUM_SECTORS);
        freeMap->FetchFrom(freeMapFile);
        sector = freeMap->Find();  // Find a sector to hold the file header.
        if (sector == -1){
            DEBUG('f',"No encontro un sector libre\n");
            success = false;  // No free block for file header.
	
	}
        else if (!directory->Add(name, sector,freeMap)){
	    DEBUG('f',"No hay espacio en el directorio\n"); 
            success = false;  // No space in directory.
	
	}

        else {            
	    directory ->FetchFrom(directoryFile);
            header = new FileHeader;
            success = true;
            // Everthing worked, flush all changes back to disk.
	    DEBUG('f',"Se creo el archivo %s en sector %u\n",name,sector);
	    directory -> WriteBack(directoryFile);
            freeMap->WriteBack(freeMapFile);
	    header -> WriteBack(sector);	    	  
	    delete header;
        }
        delete freeMap;
    }
    DEBUG('f',"%s creado con exito\n",name);    
    delete directory;
    return success;
}

/// Open a file for reading and writing.
///
/// To open a file:
/// 1. Find the location of the file's header, using the directory.
/// 2. Bring the header into memory.
///
/// * `name` is the text name of the file to be opened.
OpenFile *
FileSystem::Open(const char *name)
{
    ASSERT(name != nullptr);
    //Abrir directorio root
    Directory *directory = new Directory(NUM_DIR_ENTRIES,DIRECTORY_SECTOR,-1);
    OpenFile  *openFile = nullptr;
    int        sector;


    directory->FetchFrom(directoryFile);
    sector = directory->Find(name,false);    
    DEBUG('x', "Opening file %s en sector %u\n", name,sector);
    if (sector >= 0){	
        LockWR* fileLock;
         // `name` was found in directory.
	// Chequear si el archivo ya fue abierto
	if (!openFileTable -> HasVal(sector)){
		DEBUG('f',"Aca entra el archivo se guarda en el sector %u\n",sector);
                fileLock = new LockWR(name); // Lock para sincronización entre lectores y escritores
		fileLock -> Opener();
		openFileTable -> Add(sector,fileLock);
		openFile = new OpenFile(sector);

	}
	else{
	   // El archivo ya se encuentra abierto	   
	   fileLock = openFileTable -> Get(sector);
	   // Si nadie está borrando el archivo, puede abrirse
	   if(fileLock -> Opener())
              openFile = new OpenFile(sector); 
	}
    }
    delete directory;
    return openFile;  // Return null if not found.
}

/// Delete a file from the file system.
///
/// This requires:
/// 1. Remove it from the directory.
/// 2. Delete the space for its header.
/// 3. Delete the space for its data blocks.
/// 4. Write changes to directory, bitmap back to disk.
///
/// Return true if the file was deleted, false if the file was not in the
/// file system.
///
/// * `name` is the text name of the file to be removed.
bool
FileSystem::Remove(const char *path)
{
    ASSERT(path != nullptr);
    
    Directory  *directory;
    Bitmap     *freeMap;
    FileHeader *fileHeader;
    int         sector;    
    LockWR* lockwr = nullptr;
    directory = new Directory(NUM_DIR_ENTRIES,DIRECTORY_SECTOR,-1);
    directory->FetchFrom(directoryFile);
    //Buscar primero como archivo
    sector = directory->Find(path,false);       
    DEBUG('b',"sec:%u\n",sector);
    if (sector == -1) {
       //Buscar como directorio
       sector = directory -> Find(path,true);
       if (sector == -1){
              delete directory;
              return false;  // file not found
       }
       RemoveDirContents(path,sector);

    }
    else {
	 lockwr = openFileTable -> Get(sector);
         if (lockwr != nullptr){ // El archivo se encuentran abierto
             lockwr-> Remover();
	     openFileTable -> Remove(sector);
	     delete lockwr;
	     } // Esperar que los que hayan abierto el archivo lo cierren para poder reclamar los bloques
    }

    fileHeader = new FileHeader;
    fileHeader->FetchFrom(sector);
    freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);
    fileHeader->Deallocate(freeMap);  // Remove data blocks.
    freeMap->Clear(sector);           // Remove header block.
    directory->Remove(path);
    directory -> FetchFrom(directoryFile);
    freeMap->WriteBack(freeMapFile);      // Flush to disk.
    directory->WriteBack(directoryFile);  // Flush to disk.
    delete fileHeader;
    delete directory;
    delete freeMap;
    return true;
}



void
FileSystem::RemoveDirContents(const char* path,unsigned sector){
   Directory* dir = new Directory(NUM_DIR_ENTRIES,sector,0);
   OpenFile* dirFile = new OpenFile(sector);
   dir -> FetchFrom(dirFile);
   dir -> RemoveContents(path);
   delete dir;
   delete dirFile;
}

// Listar todos los archivos en el directorio ubicado en path
void
FileSystem::List(const char* path)
{
    Directory *rootDir = new Directory(NUM_DIR_ENTRIES,DIRECTORY_SECTOR,-1);
    rootDir -> FetchFrom(directoryFile);
    // Si path == "" se imprime el contenido del directorio root
    if (!strcmp(path,"")){
	   rootDir -> List();
	   delete rootDir;
           return;	   
    }

    // Sino hay que buscar el directorio
    int sector = rootDir -> Find(path,true);
    if (sector != -1){
	    //Acá le pasamos 0 como directorio padre total despues se pisa
	    Directory* dir = new Directory(NUM_DIR_ENTRIES,sector,0);
	    OpenFile* dirHeader = new OpenFile(sector);
	    dir -> FetchFrom(dirHeader);
	    dir -> List();
	    delete dirHeader;
	    delete dir;
            return;	    
    }
    else    
       DEBUG('f',"%s no existe\n",path);
    delete rootDir;
}

void 
FileSystem::PrintDir(){
   Directory* directory = new Directory(NUM_DIR_ENTRIES,DIRECTORY_SECTOR,-1);
   directory -> FetchFrom(directoryFile);
   directory -> Print("root"," ");
   delete directory;
}


static bool
AddToShadowBitmap(unsigned sector, Bitmap *map)
{
    ASSERT(map != nullptr);

    if (map->Test(sector)) {
        DEBUG('f', "Sector %u was already marked.\n", sector);
        return false;
    }
    map->Mark(sector);
    DEBUG('f', "Marked sector %u.\n", sector);
    return true;
}

static bool
CheckForError(bool value, const char *message)
{
    if (!value)
        DEBUG('f', message);
    return !value;
}

static bool
CheckSector(unsigned sector, Bitmap *shadowMap)
{
    bool error = false;

    error |= CheckForError(sector < NUM_SECTORS, "Sector number too big.\n");
    error |= CheckForError(AddToShadowBitmap(sector, shadowMap),
                           "Sector number already used.\n");
    return error;
}

static bool
CheckFileHeader(const RawFileHeader *rh, unsigned num, Bitmap *shadowMap)
{
    ASSERT(rh != nullptr);

    bool error = false;

    DEBUG('f', "Checking file header %u.  File size: %u bytes, number of sectors: %u.\n",
          num, rh->numBytes, rh->numSectors);
    error |= CheckForError(rh->numSectors >= DivRoundUp(rh->numBytes,
                                                        SECTOR_SIZE),
                           "Sector count not compatible with file size.\n");
    error |= CheckForError(rh->numSectors < NUM_DIRECT,
		           "Too many blocks.\n");
    for (unsigned i = 0; i < rh->numSectors; i++) {
        /*unsigned s = rh->dataSectors[i];
        error |= CheckSector(s, shadowMap); falta arreglar esto*/
    }
    return error;
}

static bool
CheckBitmaps(const Bitmap *freeMap, const Bitmap *shadowMap)
{
    bool error = false;
    for (unsigned i = 0; i < NUM_SECTORS; i++) {
        DEBUG('f', "Checking sector %u. Original: %u, shadow: %u.\n",
              i, freeMap->Test(i), shadowMap->Test(i));
        error |= CheckForError(freeMap->Test(i) == shadowMap->Test(i),
                               "Inconsistent bitmap.");
    }
    return error;
}

static bool
CheckDirectory(const RawDirectory *rd, Bitmap *shadowMap)
{
    ASSERT(rd != nullptr);
    ASSERT(shadowMap != nullptr);

    bool error = false;
    unsigned nameCount = 0;
    const char *knownNames[NUM_DIR_ENTRIES];

    for (unsigned i = 0; i < NUM_DIR_ENTRIES; i++) {
        DEBUG('f', "Checking direntry: %u.\n", i);
        const DirectoryEntry *e = &rd->table[i];

        if (e->inUse) {
            if (strlen(e->name) > FILE_NAME_MAX_LEN) {
                DEBUG('f', "Filename too long.\n");
                error = true;
            }

            // Check for repeated filenames.
            DEBUG('f', "Checking for repeated names.  Name count: %u.\n",
                  nameCount);
            bool repeated = false;
            for (unsigned j = 0; j < nameCount; j++) {
                DEBUG('f', "Comparing \"%s\" and \"%s\".\n",
                      knownNames[j], e->name);
                if (strcmp(knownNames[j], e->name) == 0) {
                    DEBUG('f', "Repeated filename.\n");
                    repeated = true;
                    error = true;
                }
            }
            if (!repeated) {
                knownNames[nameCount] = e->name;
                DEBUG('f', "Added \"%s\" at %u.\n", e->name, nameCount);
                nameCount++;
            }

            // Check sector.
            error |= CheckSector(e->sector, shadowMap);

            // Check file header.
            FileHeader *h = new FileHeader;
            const RawFileHeader *rh = h->GetRaw();
            h->FetchFrom(e->sector);
            error |= CheckFileHeader(rh, e->sector, shadowMap);
            delete h;
        }
    }
    return error;
}

bool
FileSystem::Check()
{
    DEBUG('f', "Performing filesystem check\n");
    bool error = false;

    Bitmap *shadowMap = new Bitmap(NUM_SECTORS);
    shadowMap->Mark(FREE_MAP_SECTOR);
    shadowMap->Mark(DIRECTORY_SECTOR);

    DEBUG('f', "Checking bitmap's file header.\n");

    FileHeader *bitH = new FileHeader;
    const RawFileHeader *bitRH = bitH->GetRaw();
    bitH->FetchFrom(FREE_MAP_SECTOR);
    DEBUG('f', "  File size: %u bytes, expected %u bytes.\n"
               "  Number of sectors: %u, expected %u.\n",
          bitRH->numBytes, FREE_MAP_FILE_SIZE,
          bitRH->numSectors, FREE_MAP_FILE_SIZE / SECTOR_SIZE);
    error |= CheckForError(bitRH->numBytes == FREE_MAP_FILE_SIZE,
                           "Bad bitmap header: wrong file size.\n");
    error |= CheckForError(bitRH->numSectors == FREE_MAP_FILE_SIZE / SECTOR_SIZE,
                           "Bad bitmap header: wrong number of sectors.\n");
    error |= CheckFileHeader(bitRH, FREE_MAP_SECTOR, shadowMap);
    delete bitH;

    DEBUG('f', "Checking directory.\n");

    FileHeader *dirH = new FileHeader;
    const RawFileHeader *dirRH = dirH->GetRaw();
    dirH->FetchFrom(DIRECTORY_SECTOR);
    error |= CheckFileHeader(dirRH, DIRECTORY_SECTOR, shadowMap);
    delete dirH;

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);
    Directory *dir = new Directory(NUM_DIR_ENTRIES,DIRECTORY_SECTOR,-1);
    const RawDirectory *rdir = dir->GetRaw();
    dir->FetchFrom(directoryFile);
    error |= CheckDirectory(rdir, shadowMap);
    delete dir;

    // The two bitmaps should match.
    DEBUG('f', "Checking bitmap consistency.\n");
    error |= CheckBitmaps(freeMap, shadowMap);
    delete shadowMap;
    delete freeMap;

    DEBUG('f', error ? "Filesystem check succeeded.\n"
                     : "Filesystem check failed.\n");

    return !error;
}

/// Print everything about the file system:
/// * the contents of the bitmap;
/// * the contents of the directory;
/// * for each file in the directory:
///   * the contents of the file header;
///   * the data in the file.
void
FileSystem::Print()
{
    FileHeader *bitHeader = new FileHeader;
    FileHeader *dirHeader = new FileHeader;
    Bitmap     *freeMap   = new Bitmap(NUM_SECTORS);

    printf("--------------------------------\n"
           "Bit map file header:\n\n");
    bitHeader->FetchFrom(FREE_MAP_SECTOR);
    bitHeader->Print();

    printf("--------------------------------\n"
           "Directory file header:\n\n");
    dirHeader->FetchFrom(DIRECTORY_SECTOR);
    dirHeader->Print();

    printf("--------------------------------\n");
    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    
    printf("--------------------------------\n");
    printf("Árbol del directorio\n");
    PrintDir();
    printf("--------------------------------\n");

    delete bitHeader;
    delete dirHeader;
    delete freeMap;
}

OpenFile* FileSystem::GetFreeMap(){
 return freeMapFile;
}

// Para comprobar que un directorio existe basta
// buscar el sector en el que se encuentra
// -1 indica que no existe
bool FileSystem::DirectoryExist(const char* path){
    DEBUG('f',"busco %s\n",path);
    Directory* dir = new Directory(NUM_DIR_ENTRIES,DIRECTORY_SECTOR,-1);
    dir -> FetchFrom(directoryFile);    
    int res = dir -> Find(path,true);
    DEBUG('f',"res:%d\n",res);
    delete dir;
    return res != -1;
}

