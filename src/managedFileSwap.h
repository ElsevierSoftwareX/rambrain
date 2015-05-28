#ifndef MANAGEDFILESWAP_H
#define MANAGEDFILESWAP_H

#include "managedSwap.h"
#include <stdio.h>
#include <unistd.h>
#include <map>
#include <aio.h>
#include <signal.h>

//Test classes
class managedFileSwap_Unit_SwapAllocation_Test;
class managedFileSwap_Integration_RandomAccess_Test;
class managedFileSwap_Integration_RandomAccessVariousSize_Test;

namespace membrain
{

enum pageChunkStatus {PAGE_FREE = 1,
                      PAGE_PART = 2,
                      PAGE_END = 4,
                      PAGE_WASREAD = 8,
                      PAGE_UNKNOWN_STATE = 16
                     };

typedef uint64_t global_offset;


//This will be stored in managedMemoryChunk::swapBuf :
class pageFileLocation{
public:
    pageFileLocation(unsigned int file, global_bytesize offset,global_bytesize size, pageChunkStatus status=PAGE_FREE): file(file), offset(offset),size(size),glob_off_next(NULL),status(status),aio_ptr(NULL) {};
    unsigned int file;
    global_bytesize offset;
    global_bytesize size;
    pageFileLocation *glob_off_next;//This points if used to the next part, if free to the next free chunk.
    pageChunkStatus status;
    struct aiocb *aio_ptr;
};



class managedFileSwap;


class managedFileSwap : public managedSwap
{
public:

    managedFileSwap ( global_bytesize size, const char *filemask, global_bytesize oneFile = 0 );
    ~managedFileSwap();

    virtual void swapDelete ( managedMemoryChunk *chunk );
    virtual unsigned int swapIn ( managedMemoryChunk **chunklist, unsigned int nchunks );
    virtual bool swapIn ( managedMemoryChunk *chunk );
    virtual unsigned int swapOut ( managedMemoryChunk **chunklist, unsigned int nchunks );
    virtual bool swapOut ( managedMemoryChunk *chunk );

    const unsigned int pageSize;

private:
    pageFileLocation determinePFLoc ( global_offset g_offset, global_bytesize length );
    inline global_offset determineGlobalOffset ( const pageFileLocation &ref );
    bool openSwapFiles();
    void closeSwapFiles();

    const char *filemask;

    unsigned int pageFileNumber;
    global_bytesize pageFileSize;

    unsigned int windowSize;
    unsigned int windowNumber;
    unsigned int lastCreatedWindow = 0;

    FILE **swapFiles = NULL;

    //Memory copy:
    void scheduleCopy(membrain::pageFileLocation& ref, void* ramBuf, bool reverse = false);
    inline void scheduleCopy(void *ramBuf, pageFileLocation &ref) {scheduleCopy(ref,ramBuf,true);};
    void copyMem ( void* ramBuf, membrain::pageFileLocation& ref );
    void copyMem ( membrain::pageFileLocation& ref, void* ramBuf );


    bool filesOpen = false;


    //page file malloc:
    pageFileLocation *pfmalloc ( global_bytesize size );
    void pffree ( pageFileLocation *pagePtr );
    pageFileLocation *allocInFree ( pageFileLocation *freeChunk, global_bytesize size );

    std::map<global_offset, pageFileLocation *> free_space;
    std::map<global_offset, pageFileLocation *> all_space;

    
    //sigEvent Handler:
    struct sigevent evhandler;
    void asyncIoArrived(union sigval &signal);
public:
    static void	staticAsyncIoArrived(union sigval signal) {instance->asyncIoArrived(signal);};
protected:
    //Test classes
    friend class ::managedFileSwap_Unit_SwapAllocation_Test;
    friend class ::managedFileSwap_Integration_RandomAccess_Test;
    friend class ::managedFileSwap_Integration_RandomAccessVariousSize_Test;

    static managedFileSwap *instance;
    static void sigStat ( int signum );
};

}


#endif
