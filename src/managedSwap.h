#ifndef MANAGEDSWAP_H
#define MANAGEDSWAP_H

#include "managedMemoryChunk.h"
#include "common.h"
#include "managedMemory.h"
#include "membrain_atomics.h"

namespace membrain
{

class managedSwap
{
public:
    managedSwap ( global_bytesize size );
    virtual ~managedSwap();

    //Returns number of sucessfully swapped chunks
    virtual global_bytesize swapOut ( managedMemoryChunk **chunklist, unsigned int nchunks ) = 0;
    virtual global_bytesize swapOut ( managedMemoryChunk *chunk )  = 0;
    virtual global_bytesize swapIn ( managedMemoryChunk **chunklist, unsigned int nchunks ) = 0;
    virtual global_bytesize swapIn ( managedMemoryChunk *chunk ) = 0;
    virtual void swapDelete ( managedMemoryChunk *chunk ) = 0;

    virtual inline global_bytesize getSwapSize() {
        return swapSize;
    }
    virtual inline global_bytesize getUsedSwap() {
        return swapUsed;
    }
    virtual inline global_bytesize getFreeSwap() {
        return swapFree;
    }
    inline size_t getMemoryAlignment() {
        return memoryAlignment;
    }
    void claimUsageof ( global_bytesize bytes, bool rambytes, bool used ) {
        managedMemory::defaultManager->claimUsageof ( bytes, rambytes, used );
        if ( !rambytes ) {
            swapFree += ( used ? -bytes : bytes );
            swapUsed += ( used ? bytes : -bytes );
        }
    };
    /** Function waits for all asynchronous IO to complete.
      * The wait is implemented non-performant as a normal user does not have to wait for this.
      * Implementing this with a _cond just destroys performance in the respective swapIn/out procedures without increasing any user space functionality. **/
    void waitForCleanExit() {
        printf ( "\n" );
        while ( totalSwapActionsQueued != 0 ) {
            checkForAIO();
            printf ( "waiting for aio to complete on %d objects\r", totalSwapActionsQueued );
        };
        printf ( "                                                       \r" );
    };

    virtual bool checkForAIO() {
        return false;
    };

protected:


    global_bytesize swapSize;
    global_bytesize swapUsed;
    global_bytesize swapFree;
    unsigned int totalSwapActionsQueued = 0;

    size_t memoryAlignment = 1;
};


}




#endif






