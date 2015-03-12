#include "cyclicManagedMemory.h"
#include "common.h"

cyclicManagedMemory::cyclicManagedMemory (managedSwap *swap, unsigned int size) : managedMemory ( swap,size )
{

}

void cyclicManagedMemory::schedulerRegister ( managedMemoryChunk& chunk )
{
    cyclicAtime *neu = new cyclicAtime;

    //Couple chunk to atime and vice versa:
    neu->chunk = &chunk;
    chunk.schedBuf = ( void * ) neu;

    if ( active == NULL ) { // We're inserting first one
        neu->prev = neu->next = neu;
        counterActive = neu;
    } else { //We're inserting somewhen.
        cyclicAtime *before = active->prev;
        cyclicAtime *after = active;
        MUTUAL_CONNECT(before,neu);
        MUTUAL_CONNECT(neu,after);
    }
    active = neu;
}



void cyclicManagedMemory::schedulerDelete ( managedMemoryChunk& chunk )
{
    cyclicAtime *element = ( cyclicAtime * ) chunk.schedBuf;

    //Hook out element:
    if ( element->next==element->prev ) {
        active = NULL;
        counterActive=NULL;
        return;

    }
    element->next->prev = element->prev;
    element->prev->next = element->next;

    if ( chunk.status==MEM_SWAPPED ) {
        memory_swapped -= chunk.size;
    }
    if(active==element)
        active=element->next;

    if(counterActive==element)
        counterActive=element->prev;

    delete element;
}
//Touch happens automatically after use, create, swapIn
bool cyclicManagedMemory::touch ( managedMemoryChunk& chunk )
{
    chunk.atime = atime++;
    
    //Put this object to begin of loop:
    cyclicAtime *element = (cyclicAtime *)chunk.schedBuf;
    
    if(counterActive==element);
        counterActive=counterActive->prev;
    
    if(active==element)
        return true;
    if(active->prev == element){
        active = element;
        return true;
    };
    if(active->next == element){
        cyclicAtime *before = active->prev;
        cyclicAtime *after = element->next;
        MUTUAL_CONNECT(before,element);
        MUTUAL_CONNECT(element,active);
        MUTUAL_CONNECT(active,after);
        active = element;
    }
    
    
    
    cyclicAtime *oldplace_before = element->prev;
    cyclicAtime *oldplace_after = element->next;
    cyclicAtime *before = active->prev;
    cyclicAtime *after = active->next;
    
    MUTUAL_CONNECT(oldplace_before,oldplace_after);
    MUTUAL_CONNECT(before,element);
    MUTUAL_CONNECT(element,after);
    active = element;
    return true;
}


bool cyclicManagedMemory::swapIn ( managedMemoryChunk& chunk )
{
    ensureEnoughSpaceFor(chunk.size);
    if(swap->swapIn(&chunk)) {
        memory_used+= chunk.size;
        memory_swapped-= chunk.size;
#ifdef SWAPSTATS
        swap_in_bytes+= chunk.size;
        n_swap_in+=1;
#endif
        if(chunk.schedBuf==counterActive)
            counterActive=counterActive->prev;
        return true;
    } else {
        return false;
    };

}


//Idea: swap out more than required, as the free space may be filled with premptive swap-ins

bool cyclicManagedMemory::checkCycle() {
    unsigned int no_reg = memChunks.size()-1;
    unsigned int encountered = 0;
    cyclicAtime *cur = active;
    cyclicAtime *oldcur;
    do{
        ++encountered;
        oldcur = cur;
        cur = cur->next;
        if(oldcur!=cur->prev){
            errmsg("Mutual connecion failure");
            return false;
        }
        
    }while(cur!=active);
    
    if(encountered!=no_reg)
    {
        errmsgf("Not all elements accessed. %d expected,  %d encountered",no_reg,encountered);
        return false;
    }else
        return true;

}


void cyclicManagedMemory::printCycle()
{
    cyclicAtime *atime = active;
    checkCycle();
    printf("%d (%d)<-counterActive\n",counterActive->chunk->id,counterActive->chunk->atime);
    printf("%d => %d => %d\n",counterActive->prev->chunk->id,counterActive->chunk->id,counterActive->next->chunk->id);
    printf("%d (%d)<-active\n",active->chunk->id,active->chunk->atime);
    printf("%d => %d => %d\n",active->prev->chunk->id,active->chunk->id,active->next->chunk->id);
    printf("\n");
    do {
        char  status[2];
        status[1] = 0x00;
        switch(atime->chunk->status) {
        case MEM_ALLOCATED:
            status[0] = 'A';
            break;
        case MEM_SWAPPED:
            status[0] = 'S';
            break;
        case MEM_ALLOCATED_INUSE:
            status[0] = 'U';
            break;
        case MEM_ROOT:
            status[0] = 'R';
            break;
        }
        if(atime == counterActive)
            printf("%d (%d) %s <-counterActive\n",atime->chunk->id,atime->chunk->atime,status);
        else
            printf("%d (%d) %s \n",atime->chunk->id,atime->chunk->atime,status);
        atime=atime->next;
    } while(atime!=active);
    printf("\n");
    printf("\n");
}



bool cyclicManagedMemory::swapOut ( unsigned int min_size )
{
    if (min_size>memory_max)
        return false;
    unsigned int mem_alloc_max = memory_max*swapOutFrac; //<- This is target size
    unsigned int mem_swap_min = memory_used>mem_alloc_max?memory_used-mem_alloc_max:0;
    unsigned int mem_swap = mem_swap_min<min_size?min_size:mem_swap_min;

    cyclicAtime *fromPos = counterActive;
    cyclicAtime *countPos = counterActive;
    unsigned int unload_size=0,unload=0;
    unsigned int passed=0;
    while(unload_size<mem_swap) {
        ++passed;
        if(countPos->chunk->status==MEM_ALLOCATED) {
            unload_size+=countPos->chunk->size;
            ++unload;
        }
        countPos=countPos->prev;
        if(fromPos==countPos)
            break;

    }
    if(fromPos==countPos) { //We've been round one time and could not make it.
        printTree();
        errmsgf("Cannot swap as too much memory is used to satisfy swap requirement.\n\tHappened after passing %d elements",passed);
        printCycle();
        return false;
    }

    managedMemoryChunk *unloadlist[unload];
    managedMemoryChunk **unloadElem = unloadlist;

    while(fromPos!=countPos) {
        if(fromPos->chunk->status==MEM_ALLOCATED) {
            *unloadElem = fromPos->chunk;
            ++unloadElem;
        }
        fromPos = fromPos->prev;
    }
    bool swapSuccess = (swap->swapOut(unloadlist,unload)==unload) ;
    fromPos = counterActive;
    cyclicAtime *moveEnd, *cleanFrom;
    moveEnd=NULL;
    cleanFrom = counterActive;
    bool inSwappedSection=true;
    
    //TODO: Implement this for less than 3 elements!
    
    while(fromPos!=countPos) {
        if(inSwappedSection) {
            if(fromPos->chunk->status!=MEM_SWAPPED) {
                inSwappedSection=false;
                if(moveEnd){
                     //  xxxxxxxxxxoooooooxxxxxxooooooo
                     //  ------A--><--B--><-C--><--D
                     // Change order from A-B-C-D to A-C-B-D
                                  
                    cyclicAtime *endNonswap = fromPos; //A
                    cyclicAtime *startIsoswap = fromPos->next; //B
                    cyclicAtime *endIsoswap = moveEnd; //B
                    cyclicAtime *startIsoNonswap = moveEnd->next;//C
                    cyclicAtime *endIsoNonswap = cleanFrom->prev;//C
                    cyclicAtime *startClean = cleanFrom;//D
                    //A-C:
                    MUTUAL_CONNECT(endNonswap,startIsoNonswap);
                    MUTUAL_CONNECT(endIsoNonswap,startIsoswap);
                    MUTUAL_CONNECT(endIsoswap,startClean);
                    cleanFrom=startIsoswap;
                    moveEnd=NULL;               
                }
            }
            else{
                if(moveEnd)
                    cleanFrom = fromPos;
            }
        } else {
            if(fromPos->chunk->status==MEM_SWAPPED) {
                inSwappedSection=true;
                if(moveEnd==NULL)
                    moveEnd=fromPos;
            }
        }
        fromPos = fromPos->prev;
    }
    if(moveEnd){
        //  xxxxxxxxxxoooooooxxxxxxooooooo
        //  ------A--><--B--><-C--><--D
        // Change order from A-B-C-D to A-C-B-D
        
        cyclicAtime *endNonswap = fromPos; //A
        cyclicAtime *startIsoswap = fromPos->next; //B
        cyclicAtime *endIsoswap = moveEnd; //B
        cyclicAtime *startIsoNonswap = moveEnd->next;//C
        cyclicAtime *endIsoNonswap = cleanFrom->prev;//C
        cyclicAtime *startClean = cleanFrom;//D
        //A-C:
        MUTUAL_CONNECT(endNonswap,startIsoNonswap);
        MUTUAL_CONNECT(endIsoNonswap,startIsoswap);
        MUTUAL_CONNECT(endIsoswap,startClean);
        cleanFrom=startIsoswap;
        moveEnd=NULL;               
    }
    counterActive = cleanFrom;
    
    if(swapSuccess) {

        memory_swapped+=unload_size;
        memory_used-=unload_size;
#ifdef SWAPSTATS
        swap_out_bytes+= unload_size;
        n_swap_out+=1;
#endif
        return true;
    }

    else {
        return false;
    }
}

