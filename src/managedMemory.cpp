#include "managedMemory.h"
#include "common.h"
#include "exceptions.h"
#include "dummyManagedMemory.h"
#include <sys/signal.h>
#include "initialisation.h"
#include "membrain_atomics.h"
#include "git_info.h"

namespace membrain
{
namespace membrainglobals
{
membrainConfig config;
}

managedMemory *managedMemory::defaultManager;

#ifdef PARENTAL_CONTROL
memoryID const managedMemory::root = 1;
memoryID managedMemory::parent = managedMemory::root;
bool managedMemory::threadSynced = false;
pthread_mutex_t managedMemory::parentalMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t managedMemory::parentalCond = PTHREAD_COND_INITIALIZER;
pthread_t managedMemory::creatingThread ;
bool managedMemory::haveCreatingThread = false;
#endif
memoryID const managedMemory::invalid = 0;



bool managedMemory::noThrow = false;
pthread_mutex_t managedMemory::topologicalMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t managedMemory::swappingMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t managedMemory::stateChangeMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t managedMemory::swappingCond = PTHREAD_COND_INITIALIZER;
unsigned int managedMemory::arrivedSwapins = 0;

managedMemory::managedMemory ( managedSwap *swap, unsigned int size  )
{
    memory_max = size;
    previousManager = defaultManager;
    defaultManager = this;
#ifdef PARENTAL_CONTROL
    managedMemoryChunk *chunk = mmalloc ( 0 );                //Create root element.
    chunk->status = MEM_ROOT;
    haveCreatingThread = false;

#endif
    this->swap = swap;
    if ( !swap ) {
        throw incompleteSetupException ( "no swap manager defined" );
    }
#ifdef SWAPSTATS
    instance = this;
    signal ( SIGUSR1, sigswapstats );
#endif
}

managedMemory::~managedMemory()
{
    bool oldthrow = noThrow;
    noThrow = true;
    if ( defaultManager == this ) {

        defaultManager = previousManager;
    }
#ifdef PARENTAL_CONTROL
    //Clean up objects:
    recursiveMfree ( root );
#else
    linearMfree();
#endif
    noThrow = oldthrow;
}


unsigned int managedMemory::getMemoryLimit () const
{
    return memory_max;
}
unsigned int managedMemory::getUsedMemory() const
{
    return memory_used;
}
unsigned int managedMemory::getSwappedMemory() const
{
    return memory_swapped;
}



bool managedMemory::setMemoryLimit ( unsigned int size )
{
    if ( size > memory_max ) {
        memory_max = size;
        return true;
    } else {
        if ( size < memory_used ) {
            //Try to swap out as much memory as needed:
            unsigned int tobefreed = ( memory_used - size );
            if ( !swapOut ( tobefreed ) ) {
                return false;
            }
            memory_max = size;
            swapOut ( 0 ); // Swap out further to adapt to new memory limits. This must not necessarily succeed.
            return true;
        } else {
            memory_max = size;
            return true;
        }
    }
    return false;                                             //Resetting this is not implemented yet.
}

void managedMemory::ensureEnoughSpaceAndLockTopo ( unsigned int sizereq )
{
    pthread_mutex_lock ( &topologicalMutex ); // We want to change memory topology, as we want to add an element
    if ( sizereq + memory_used > memory_max ) {
        if ( !swapOut ( sizereq + memory_used - memory_max ) ) { //Execute swapOut in protected context
            pthread_mutex_unlock ( &topologicalMutex );
            Throw ( memoryException ( "Could not swap memory" ) );
        } else {
            //We'll wait for possible swapOut to actually occur:

            pthread_mutex_lock ( &swappingMutex );
            while ( arrivedSwapins == 0 || ( sizereq + memory_used > memory_max ) ) {
                pthread_cond_wait ( &swappingCond, &swappingMutex );
            }
            arrivedSwapins -= 1;
            pthread_mutex_unlock ( &topologicalMutex );
            pthread_mutex_unlock ( &swappingMutex );

        }
    }
}


managedMemoryChunk *managedMemory::mmalloc ( unsigned int sizereq )
{
    ///\todo: The following function should obtain the topologicalLock for our action:
    ensureEnoughSpaceAndLockTopo ( sizereq );

    memory_used += sizereq;

    //We are left with enough free space to malloc.
#ifdef PARENTAL_CONTROL
    managedMemoryChunk *chunk = new managedMemoryChunk ( parent, memID_pace++ );
#else
    managedMemoryChunk *chunk = new managedMemoryChunk ( memID_pace++ );
#endif
    chunk->status = MEM_ALLOCATED;
    chunk->size = sizereq;
#ifdef PARENTAL_CONTROL
    chunk->child = invalid;
    chunk->parent = parent;
    chunk->swapBuf = NULL;
#endif
#ifdef PARENTAL_CONTROL
    if ( chunk->id == root ) {                                //We're inserting root elem.

        chunk->next = invalid;

        chunk->atime = 0;
    } else {
#endif
        //Register this chunk in swapping logic:
        chunk->atime = atime++;
        schedulerRegister ( *chunk );
        touch ( *chunk );
#ifdef PARENTAL_CONTROL
        //fill in tree:
        managedMemoryChunk &pchunk = resolveMemChunk ( parent );
        if ( pchunk.child == invalid ) {
            chunk->next = invalid;                                //Einzelkind
            pchunk.child = chunk->id;
        } else {
            chunk->next = pchunk.child;
            pchunk.child = chunk->id;
        }

    }
#endif

    memChunks.insert ( {chunk->id, chunk} );
    if ( sizereq != 0 ) {
        chunk->locPtr = malloc ( sizereq );
        if ( !chunk->locPtr ) {
            Throw ( memoryException ( "Malloc failed" ) );
            pthread_mutex_unlock ( &topologicalMutex );
            return NULL;
        }
    }
    pthread_mutex_unlock ( &topologicalMutex );
    return chunk;
}

bool managedMemory::swapIn ( memoryID id )
{
    managedMemoryChunk chunk = resolveMemChunk ( id );
    return swapIn ( chunk );
}

bool managedMemory::setUse ( managedMemoryChunk &chunk, bool writeAccess = false )
{
    pthread_mutex_lock ( &stateChangeMutex );
    switch ( chunk.status ) {
    case MEM_SWAPIN: // Wait for object to appear
        while ( true ) {
            pthread_cond_wait ( &swappingCond, &stateChangeMutex );
            if ( chunk.status == MEM_ALLOCATED || ( ( chunk.status ) &MEM_ALLOCATED_INUSE_READ ) ) {
                break;//This continues with status change when object is at least allocated.
            }
        }
    case MEM_ALLOCATED:
        chunk.status = MEM_ALLOCATED_INUSE_READ;
    case MEM_ALLOCATED_INUSE_READ:
        if ( writeAccess ) {
            chunk.status = MEM_ALLOCATED_INUSE_WRITE;
        }
    case MEM_ALLOCATED_INUSE_WRITE:

#ifdef SWAPSTATS
        ++swap_hits;
#endif
        ++chunk.useCnt;
        pthread_mutex_unlock ( &stateChangeMutex );
        touch ( chunk );
        return true;
//Id this thing is swapped or about to be swapped, "call it back"
    case MEM_SWAPPED:
    case MEM_SWAPOUT:
#ifdef SWAPSTATS
        ++swap_misses;
        --swap_hits;
#endif
        pthread_mutex_unlock ( &stateChangeMutex );
        if ( swapIn ( chunk ) ) {

            return setUse ( chunk , writeAccess );
        } else {
            return Throw ( memoryException ( "Could not swap memory" ) );
        }



    case MEM_ROOT:
        pthread_mutex_unlock ( &stateChangeMutex );
        return false;

    }
    pthread_mutex_unlock ( &stateChangeMutex );
    return false;
}

bool managedMemory::mrealloc ( memoryID id, unsigned int sizereq )
{
    pthread_mutex_lock ( &topologicalMutex );
    managedMemoryChunk &chunk = resolveMemChunk ( id );
    if ( !setUse ( chunk ) ) {
        return false;
    }
    void *realloced = realloc ( chunk.locPtr, sizereq );
    if ( realloced ) {
        memory_used -= chunk.size - sizereq;
        chunk.size = sizereq;
        chunk.locPtr = realloced;
        unsetUse ( chunk );
        pthread_mutex_unlock ( &topologicalMutex );
        return true;
    } else {
        unsetUse ( chunk );
        pthread_mutex_unlock ( &topologicalMutex );
        return false;
    }
}


bool managedMemory::unsetUse ( memoryID id )
{
    managedMemoryChunk chunk = resolveMemChunk ( id );
    return unsetUse ( chunk );
}


bool managedMemory::setUse ( memoryID id )
{
    managedMemoryChunk chunk = resolveMemChunk ( id );
    return setUse ( chunk );
}

bool managedMemory::unsetUse ( managedMemoryChunk &chunk , unsigned int no_unsets )
{
    pthread_mutex_lock ( &stateChangeMutex );
    if ( chunk.status & MEM_ALLOCATED_INUSE_READ ) {
        chunk.useCnt -= no_unsets;
        chunk.status = ( chunk.useCnt == 0 ? MEM_ALLOCATED : chunk.status );
        pthread_mutex_unlock ( &stateChangeMutex );
        return true;
    } else {
        pthread_mutex_unlock ( &stateChangeMutex );
        return Throw ( memoryException ( "Can not unset use of not used memory" ) );
    }
}

bool managedMemory::Throw ( memoryException e )
{
    if ( noThrow ) {
        errmsg ( e.what() );
        return false;
    } else {
#ifdef PARENTAL_CONTROL
        if ( haveCreatingThread && pthread_equal ( pthread_self(), creatingThread ) ) {
            pthread_mutex_unlock ( &parentalMutex );
        }
#endif

        throw e;
    }
}



void managedMemory::mfree ( memoryID id )
{
    pthread_mutex_lock ( &topologicalMutex );
    managedMemoryChunk *chunk = memChunks[id];
    if ( chunk->status & MEM_ALLOCATED_INUSE_READ ) {

        Throw ( memoryException ( "Can not free memory which is in use" ) );
        pthread_mutex_unlock ( &topologicalMutex );
        return;
    }



#ifdef PARENTAL_CONTROL
    if ( chunk->child != invalid ) {
        Throw ( memoryException ( "Can not free memory which has active children" ) );
        pthread_mutex_unlock ( &topologicalMutex );
        return;
    }
    if ( chunk->id != root ) {
        schedulerDelete ( *chunk );
        if ( chunk->status == MEM_ALLOCATED ) {
            free ( chunk->locPtr );
            memory_used -= chunk->size;
        } else {
            swap->swapDelete ( chunk );
        }
        managedMemoryChunk *pchunk = &resolveMemChunk ( chunk->parent );
        if ( pchunk->child == chunk->id ) {
            pchunk->child = chunk->next;
        } else {
            pchunk = &resolveMemChunk ( pchunk->child );
            do {
                if ( pchunk->next == chunk->id ) {
                    pchunk->next = chunk->next;
                    break;
                };
                if ( pchunk->next == invalid ) {
                    break;
                } else {
                    pchunk = &resolveMemChunk ( pchunk->next );
                }
            } while ( 1 == 1 );
        }
    }
#else
    schedulerDelete ( *chunk );
    if ( chunk->status == MEM_ALLOCATED ) {
        free ( chunk->locPtr );
        memory_used -= chunk->size;
    } else {
        swap->swapDelete ( chunk );
    }
#endif
    //Delete element itself
    memChunks.erase ( id );
    delete ( chunk );
    pthread_mutex_unlock ( &topologicalMutex );
}

#ifdef PARENTAL_CONTROL

unsigned int managedMemory::getNumberOfChildren ( const memoryID &id )
{
    const managedMemoryChunk &chunk = resolveMemChunk ( id );
    if ( chunk.child == invalid ) {
        return 0;
    }
    unsigned int no = 1;
    const managedMemoryChunk *child = &resolveMemChunk ( chunk.child );
    while ( child->next != invalid ) {
        child = &resolveMemChunk ( child->next );
        no++;
    }
    return no;
}


void managedMemory::printTree ( managedMemoryChunk *current, unsigned int nspaces )
{
    if ( !current ) {
        current = &resolveMemChunk ( root );
    }
    do {
        for ( unsigned int n = 0; n < nspaces; n++ ) {
            printf ( "  " );
        }
        printf ( "(%d : size %d Bytes, atime %d, ", current->id, current->size, current->atime );
        switch ( current->status ) {
        case MEM_ROOT:
            printf ( "Root Element" );
            break;
        case MEM_SWAPPED:
            printf ( "Swapped out" );
            break;
        case MEM_ALLOCATED:
            printf ( "Allocated" );
            break;
        case MEM_ALLOCATED_INUSE_WRITE:
            printf ( "Allocated&inUse (writable)" );
            break;
        case MEM_ALLOCATED_INUSE_READ:
            printf ( "Allocated&inUse (readonly)" );
            break;
        }
        printf ( ")\n" );
        if ( current->child != invalid ) {
            printTree ( &resolveMemChunk ( current->child ), nspaces + 1 );
        }
        if ( current->next != invalid ) {
            current = &resolveMemChunk ( current->next );
        } else {
            break;
        };
    } while ( 1 == 1 );
}

///\note This function does not preserve correct deallocation order in class hierarchies, as such, it is not a valid garbage collector.
void managedMemory::recursiveMfree ( memoryID id )
{
    managedMemoryChunk *oldchunk = &resolveMemChunk ( id );
    managedMemoryChunk *next;
    do {
        if ( oldchunk->child != invalid ) {
            recursiveMfree ( oldchunk->child );
        }
        if ( oldchunk->next != invalid ) {
            next = &resolveMemChunk ( oldchunk->next );
        } else {
            break;
        }
        mfree ( oldchunk->id );
        oldchunk = next;
    } while ( 1 == 1 );
    mfree ( oldchunk->id );
}
#else
///\note This function does not preserve correct deallocation order in class hierarchies, as such, it is not a valid garbage collector.

void managedMemory::linearMfree()
{
    if ( memChunks.size() == 0 ) {
        return;
    }
    auto it = memChunks.begin();
    do {
        mfree ( it->first );
    } while ( ++it != memChunks.end() );
}

#endif
managedMemoryChunk &managedMemory::resolveMemChunk ( const memoryID &id )
{
    return *memChunks[id];
}

#ifdef SWAPSTATS
void managedMemory::printSwapstats()
{
    infomsgf ( "A total of %d swapouts occured, writing out %ld bytes (%.3e Bytes/avg)\
          \n\tA total of %d swapins occured, reading in %ld bytes (%.3e Bytes/avg)\
          \n\twe used already loaded elements %d times, %d had to be fetched\
          \n\tthus, the hits over misses rate was %.5f\
          \n\tfraction of swapped out ram (currently) %.2e", n_swap_out, swap_out_bytes, \
               ( ( float ) swap_out_bytes ) / n_swap_out, n_swap_in, swap_in_bytes, ( ( float ) swap_in_bytes ) / n_swap_in, \
               swap_hits, swap_misses, ( ( float ) swap_hits / swap_misses ), ( ( float ) memory_swapped ) / ( memory_used + memory_swapped ) );
}

void managedMemory::resetSwapstats()
{
    swap_hits = swap_misses = swap_in_bytes = swap_out_bytes = n_swap_in = n_swap_out = 0;
}

managedMemory *managedMemory::instance = NULL;
void managedMemory::sigswapstats ( int signum )
{
    printf ( "%ld\t%ld\t%ld\t%ld\t%e\n", instance->swap_out_bytes,
             instance->swap_out_bytes - instance->swap_out_bytes_last,
             instance->swap_in_bytes,
             instance->swap_in_bytes - instance->swap_in_bytes_last, ( double ) instance->swap_hits / instance->swap_misses );
    instance->swap_out_bytes_last = instance->swap_out_bytes;
    instance->swap_in_bytes_last = instance->swap_in_bytes;
}
#endif

void managedMemory::versionInfo()
{

#ifdef SWAPSTATS
    const char *swapstats = "with swapstats";
#else
    const char *swapstats = "without swapstats";
#endif
#ifdef PARENTAL_CONTROL
    const char *parentalcontrol = "with parental control";
#else
    const char *parentalcontrol = "without parental_control";
#endif

    infomsgf ( "compiled from %s\n\ton %s at %s\n\
                \t%s , %s\n\
    \n \t git diff\n%s\n", gitCommit, __DATE__, __TIME__, swapstats, parentalcontrol, gitDiff );


}

void managedMemory::signalSwappingCond()
{
    pthread_mutex_lock ( &swappingMutex );
    arrivedSwapins += 1;
    pthread_cond_broadcast ( &swappingCond );
    pthread_mutex_unlock ( &swappingMutex );
}



}



