#include "managedFileSwap.h"
#include "common.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include "exceptions.h"
managedFileSwap::managedFileSwap ( unsigned int size, const char *filemask, unsigned int oneFile ) : managedSwap ( size )
{


    //calculate page File number:
    unsigned int padding = oneFile % pageSize;
    if ( padding != 0 ) {
        warnmsgf ( "requested single swap filesize is not a multiple of pageSize.\n\t %u Bytes left over.", padding );
    }

    oneFile += pageSize - padding;
    pageFileSize = oneFile;
    if ( size % oneFile != 0 ) {
        pageFileNumber = size / oneFile + 1;
    } else {
        pageFileNumber = size / oneFile;
    }

    //initialize inherited members:
    swapSize = pageFileNumber * oneFile;
    swapUsed = 0;

    //copy filemask:
    this->filemask = ( char * ) malloc ( sizeof ( char ) * ( strlen ( filemask ) + 1 ) );
    strcpy ( ( char * ) this->filemask, filemask );

    swapFiles = NULL;
    if ( !openSwapFiles() ) {
        throw memoryException ( "Could not create swap files" );
    }

    //Initialize swapmalloc:
    for ( unsigned int n = 0; n < pageFileNumber; n++ ) {
        pageFileLocation *pfloc = new pageFileLocation;
        pfloc->file = n;
        pfloc->offset = 0;
        pfloc->size = pageFileSize;
        pfloc->glob_off_next = NULL; // We may use 0 for invalid, as the first one is never the next.
        pfloc->status = PAGE_FREE;
        free_space[n * pageFileSize] = pfloc;
        all_space[n * pageFileSize] = pfloc;
    }

    //Initialize Windows:
    unsigned int ws_ratio = .1 * pageFileSize; //TODO: unhardcode that buddy
    unsigned int ws_max = 100 * 1024 * 1024; //TODO: unhardcode that buddy
    windowNumber = 10;
    windowSize = min ( ws_max, ws_ratio );

    //Initialize one swap-window into first swap file:
    windows = ( pageFileWindow ** ) malloc ( sizeof ( pageFileWindow * ) *windowNumber );
    for ( unsigned int n = 0; n < windowNumber; ++n ) {
        windows[n] = 0;
    }
    pageFileLocation *absoluteBegin = ( free_space.begin()->second );
    windows[0] = new pageFileWindow ( *absoluteBegin, *this );
    char *test = ( char * ) windows[0]->getMem ( *absoluteBegin );
    for ( unsigned int n = 0; n < windowSize; ++n ) {
        test[n] = n % 256;
    }

}

managedFileSwap::~managedFileSwap()
{
    if ( windows ) {
        for ( unsigned int n = 0; n < windowNumber; ++n )
            if ( windows[n] ) {
                delete windows[n];
            }
        free ( windows );
    };
    closeSwapFiles();
    free ( ( void * ) filemask );
    if ( all_space.size() > 0 ) {
        std::map<unsigned int, pageFileLocation *>::iterator it = all_space.begin();
        do {
            delete it->second;
            it++;
        } while ( it != all_space.end() );
    }
}


void managedFileSwap::closeSwapFiles()
{
    if ( swapFiles ) {
        for ( unsigned int n = 0; n < pageFileNumber; ++n ) {
            fclose ( swapFiles[n] );
        }
        free ( swapFiles );
    }

}


bool managedFileSwap::openSwapFiles()
{
    if ( swapFiles ) {
        throw memoryException ( "Swap files already opened. Close first" );
        return false;
    }
    swapFiles = ( FILE ** ) malloc ( sizeof ( FILE * ) *pageFileNumber );
    for ( unsigned int n = 0; n < pageFileNumber; ++n ) {
        char fname[1024];
        snprintf ( fname, 1024, filemask, n );
        swapFiles[n] = fopen ( fname, "w+" );
        if ( !swapFiles[n] ) {
            throw memoryException ( "Could not open swap file." );
            return false;
        }
    }
    return true;
}

const unsigned int managedFileSwap::pageSize = sysconf ( _SC_PAGE_SIZE );

pageFileLocation *managedFileSwap::pfmalloc ( unsigned int size )
{
    //TODO: This may be rather stupid at the moment

    /*Priority: -Use first free chunk that fits completely
     *          -Distribute over free locations
                Later on/TODO: look for read-in memory that can be overwritten*/
    std::map<unsigned int, pageFileLocation *>::iterator it = free_space.begin();
    pageFileLocation *found = NULL;
    do {
        if ( it->second->size >= size ) {
            found = it->second;
        }
    } while ( ++it != free_space.end() );
    pageFileLocation *res = NULL;
    pageFileLocation *former = NULL;
    if ( found ) {
        pageFileLocation *res = allocInFree ( found, size );
        res->status = PAGE_END;//Don't forget to set the status of the allocated memory.
    } else { //We need to write out the data in parts.


        //check for enough space:
        unsigned int total_space = 0;
        it = free_space.begin();
        while ( true ) {
            total_space += it->second->size;
            if ( total_space >= size ) {
                break;
            }
        }
        while ( ++it != free_space.end() );
        std::map<unsigned int, pageFileLocation *>::iterator it2 = free_space.begin();


        if ( total_space >= size ) { //We can concat enough free chunks to satisfy memory requirements
            while ( true ) {
                unsigned int avail_space = it2->second->size;
                unsigned int alloc_here = min ( avail_space, total_space );
                pageFileLocation *neu = allocInFree ( it2->second, alloc_here );
                total_space -= alloc_here;
                neu->status = total_space == 0 ? PAGE_END : PAGE_PART;
                if ( !res ) {
                    res = neu;
                }
                if ( former ) {
                    former->glob_off_next = neu;
                }
                former = res;
                if ( it2 == it ) {
                    break;
                } else {
                    it++;
                }
            };

        } else {
            throw memoryException ( "Out of swap space" );
        }

    }
    return res;
}

pageFileLocation *managedFileSwap::allocInFree ( pageFileLocation *freeChunk, unsigned int size )
{
    //Hook out the block of free space:
    global_offset formerfree_off = determineGlobalOffset ( *freeChunk );
    free_space.erase ( formerfree_off );
    //We want to allocate a new chunk or use the chunk at hand.
    if ( freeChunk->size - size < sizeof ( pageFileLocation ) ) { //Memory to manage free space exceeds free space (actually more than)
        //Thus, use the free chunk for your data.
        freeChunk->size = size;
        free_space.erase ( determineGlobalOffset ( *freeChunk ) );
        return freeChunk;
    } else {
        pageFileLocation *neu = new pageFileLocation ( *freeChunk );
        freeChunk->offset += size;
        freeChunk->size -= size;
        freeChunk->glob_off_next = NULL;
        global_offset newfreeloc = determineGlobalOffset ( *freeChunk );
        free_space[newfreeloc] = freeChunk;
        neu->size = size;
        all_space[formerfree_off] = neu;
        return neu;

    }

}

void managedFileSwap::pffree ( pageFileLocation *pagePtr )
{
    pageFileLocation *free_start = pagePtr;
    while ( true ) { //We possibly need multiple frees.
        std::map<unsigned int, pageFileLocation *>::iterator it = free_space.find ( determineGlobalOffset ( *pagePtr ) );
        if ( pagePtr->status == PAGE_END || pagePtr->glob_off_next->offset == 0 || ++it->second != pagePtr->glob_off_next ) { //end of buf || end of pagefile || end of fragment reached


            pageFileLocation *cur = free_start->glob_off_next;
            pageFileLocation *next = free_start;
            if ( cur && cur != pagePtr ) //Delete all possible others.
                while ( cur != pagePtr ) {
                    next = cur->glob_off_next;
                    all_space.erase ( determineGlobalOffset ( *cur ) );
                    delete cur;
                    cur = next;
                }


            //The first page will be the chunk taking all space:
            free_start->size = pagePtr->offset + pagePtr->size - free_start->offset;
            free_space[determineGlobalOffset ( *free_start )] = free_start;


            if ( pagePtr->status == PAGE_END ) {
                break;
            }

            pagePtr = pagePtr->glob_off_next;
            free_start = pagePtr;
        } else {
            pagePtr = pagePtr->glob_off_next;
        }
    }

}


//Actual interface:
void managedFileSwap::swapDelete ( managedMemoryChunk *chunk )
{

}
bool managedFileSwap::swapIn ( managedMemoryChunk *chunk )
{

}
unsigned int managedFileSwap::swapIn ( managedMemoryChunk **chunklist, unsigned int nchunks )
{

}

bool managedFileSwap::swapOut ( managedMemoryChunk *chunk )
{
}
unsigned int managedFileSwap::swapOut ( managedMemoryChunk **chunklist, unsigned int nchunks )
{

}


global_offset managedFileSwap::determineGlobalOffset ( const pageFileLocation &ref )
{
    return ref.file * pageFileSize + ref.offset;
}



//Page File Window Class:

pageFileWindow::pageFileWindow ( const pageFileLocation &location, managedFileSwap &swap )
{
    unsigned int pm_padding = location.offset % managedFileSwap::pageSize;
    unsigned int pm_end_padding = ( location.offset + location.size ) % managedFileSwap::pageSize;
    offset = location.offset;
    length = location.size;
    file = location.file;
    if ( pm_padding > 0 ) {
        offset -= pm_padding;
        length += pm_padding;
    }
    length += pm_end_padding;
    const unsigned int fd = fileno ( swap.swapFiles[location.file] );
    //check whether swapfile is big enough, if not, rescale:
    struct stat stats;
    fstat ( fd, &stats );
    unsigned int destsize = offset + length;
    if ( stats.st_size < destsize ) {

        if ( ftruncate ( fd, destsize ) == -1 ) {
            throw memoryException ( "could not resize swap file" );
        }
    }


    buf = mmap ( NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset );
    if ( buf == MAP_FAILED ) {
        throw memoryException ( "memory map failed!" );
    }

}

pageFileWindow::~pageFileWindow()
{
    munmap ( buf, length );
}

void pageFileWindow::triggerSync ( bool async )
{
    msync ( buf, length, async ? MS_ASYNC : MS_SYNC );
}


void *pageFileWindow::getMem ( const pageFileLocation &loc )
{
    if ( loc.file != file ) {
        return NULL;
    }
    if ( loc.offset < offset ) {
        return NULL;
    }
    if ( loc.offset + loc.size > offset + length ) {
        return NULL;
    }
    return ( char * ) buf + ( loc.offset - offset );

}


