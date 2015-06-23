#include "managedSwap.h"
#include "managedMemory.h"

namespace membrain
{

managedSwap::managedSwap ( global_bytesize size ) : swapSize ( size ), swapUsed ( 0 )
{
}

managedSwap::~managedSwap()
{
    if ( managedMemory::defaultManager != NULL && managedMemory::defaultManager->swap == this ) {
        managedMemory::defaultManager->swap = NULL;
    }
}

}
