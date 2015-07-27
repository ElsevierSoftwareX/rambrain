#include "tester.h"
IGNORE_TEST_WARNINGS;

#include <gtest/gtest.h>
#include "managedPtr.h"
#include "cyclicManagedMemory.h"
#include "managedDummySwap.h"
#include "exceptions.h"

using namespace rambrain;

/**
 * @test Tests the behaviour if no memory manager is explicitely installed, ergo testing the fallback default
 */
TEST ( managedPtr, Unit_NoMemoryManager )
{
    EXPECT_NO_THROW ( managedPtr<double> ptr1 ( 10 ) );

    managedDummySwap swap ( 100 );
    cyclicManagedMemory managedMemory ( &swap, 100 );

    ASSERT_NO_THROW ( managedPtr<double> ptr2 ( 10 ) );
}


#ifdef PARENTAL_CONTROL
/**
 * @test Tests if parent is correctly assigned for the first pointer
 */
TEST ( managedPtr, Unit_ParentIDs )
{
    managedDummySwap swap ( 100 );
    cyclicManagedMemory managedMemory ( &swap, 100 );
    memoryID parent = managedMemory::defaultManager->parent;

    ASSERT_NO_THROW ( managedPtr<double> ptr ( 10 ) );
    EXPECT_EQ ( parent, managedMemory::defaultManager->parent );
}
#endif


/**
 * @test Test manual set and unsetUse on a pointer and checks if the chunks are updated appropriately
 */
TEST ( managedPtr, Unit_ChunkInUse )
{
    managedDummySwap swap ( 100 );
    cyclicManagedMemory managedMemory ( &swap, 100 );
    managedPtr<double> ptr ( 10 );

    ASSERT_EQ ( MEM_ALLOCATED, ptr.chunk->status );

    ptr.setUse();

    ASSERT_EQ ( MEM_ALLOCATED_INUSE_WRITE, ptr.chunk->status );

    ptr.unsetUse();

    ASSERT_EQ ( MEM_ALLOCATED, ptr.chunk->status );

    ptr.setUse ( true );

    ASSERT_EQ ( MEM_ALLOCATED_INUSE_WRITE, ptr.chunk->status );

    ptr.unsetUse();

    ASSERT_EQ ( MEM_ALLOCATED, ptr.chunk->status );

    ptr.setUse ( false );

    ASSERT_EQ ( MEM_ALLOCATED_INUSE_READ, ptr.chunk->status );

    ptr.unsetUse();
}


/**
 * @test Tests if a local pointer can be retreived or not if setUse is or is not done beforehand
 */
TEST ( managedPtr, Unit_GetLocPointer )
{
    managedDummySwap swap ( 100 );
    cyclicManagedMemory managedMemory ( &swap, 100 );
    managedPtr<double> ptr ( 10 );

    EXPECT_EQ ( MEM_ALLOCATED, ptr.chunk->status );
    EXPECT_THROW ( ptr.getLocPtr(), unexpectedStateException );

    ptr.setUse();

    EXPECT_EQ ( MEM_ALLOCATED_INUSE_WRITE, ptr.chunk->status );
    EXPECT_NO_THROW ( ptr.getLocPtr() );

    ptr.unsetUse();

    EXPECT_EQ ( MEM_ALLOCATED, ptr.chunk->status );
    EXPECT_THROW ( ptr.getLocPtr(), unexpectedStateException );
}


/**
 * @test Checks if chunk stati are appropriately set when handling different types of adhereto
 */
TEST ( managedPtr, Unit_SmartPointery )
{
    managedDummySwap swap ( 200 );
    cyclicManagedMemory managedMemory ( &swap, 200 );

    managedPtr<double> ptr ( 5 );
    for ( int n = 0; n < 5; n++ ) {
        EXPECT_EQ ( MEM_ALLOCATED, ptr.chunk->status );

        ADHERETOLOC ( double, ptr, lptr );
        lptr[n] = n;

        EXPECT_EQ ( MEM_ALLOCATED_INUSE_WRITE, ptr.chunk->status );
    }
    managedPtr<double> ptr2 ( 5 );
    for ( int n = 0; n < 5; n++ ) {
        EXPECT_EQ ( MEM_ALLOCATED, ptr.chunk->status );
        EXPECT_EQ ( MEM_ALLOCATED, ptr2.chunk->status );

        ADHERETOLOC ( double, ptr, lptr );
        ADHERETOLOC ( double, ptr2, lptr2 );
        lptr2[n] = 1 - n;

        EXPECT_EQ ( MEM_ALLOCATED_INUSE_WRITE, ptr.chunk->status );
        EXPECT_EQ ( MEM_ALLOCATED_INUSE_WRITE, ptr2.chunk->status );
    }

    EXPECT_EQ ( 5 * sizeof ( double ) * 2, managedMemory.getUsedMemory() );
    EXPECT_EQ ( MEM_ALLOCATED, ptr.chunk->status );
    EXPECT_EQ ( MEM_ALLOCATED, ptr2.chunk->status );

    ptr = ptr2;

    EXPECT_EQ ( 5 * sizeof ( double ), managedMemory.getUsedMemory() );
    EXPECT_EQ ( MEM_ALLOCATED, ptr.chunk->status );
    EXPECT_EQ ( MEM_ALLOCATED, ptr2.chunk->status );

    for ( int n = 0; n < 5; n++ ) {
        ADHERETOLOCCONST ( double, ptr, lptr );
        ADHERETOLOCCONST ( double, ptr2, lptr2 );

        EXPECT_EQ ( 1 - n, lptr[n] );
        EXPECT_EQ ( 1 - n, lptr2[n] );
        EXPECT_EQ ( MEM_ALLOCATED_INUSE_READ, ptr.chunk->status );
        EXPECT_EQ ( MEM_ALLOCATED_INUSE_READ, ptr2.chunk->status );
    }

    ptr = ptr2;

    for ( int n = 0; n < 5; n++ ) {
        ADHERETOLOC ( double, ptr, lptr );
        ADHERETOLOC ( double, ptr2, lptr2 );

        EXPECT_EQ ( 1 - n, lptr[n] );
        EXPECT_EQ ( 1 - n, lptr2[n] );
        EXPECT_EQ ( MEM_ALLOCATED_INUSE_WRITE, ptr.chunk->status );
        EXPECT_EQ ( MEM_ALLOCATED_INUSE_WRITE, ptr2.chunk->status );
    }
    ptr = ptr;
    for ( int n = 0; n < 5; n++ ) {
        ADHERETOLOCCONST ( double, ptr, lptr );
        ADHERETOLOCCONST ( double, ptr2, lptr2 );

        EXPECT_EQ ( 1 - n, lptr[n] );
        EXPECT_EQ ( 1 - n, lptr2[n] );
        EXPECT_EQ ( MEM_ALLOCATED_INUSE_READ, ptr.chunk->status );
        EXPECT_EQ ( MEM_ALLOCATED_INUSE_READ, ptr2.chunk->status );
    }
    managedPtr<double> ptr3 ( ptr );
    for ( int n = 0; n < 5; n++ ) {
        ADHERETOLOC ( double, ptr, lptr );
        ADHERETOLOC ( double, ptr2, lptr2 );
        ADHERETOLOC ( double, ptr3, lptr3 );

        EXPECT_EQ ( 1 - n, lptr[n] );
        EXPECT_EQ ( 1 - n, lptr2[n] );
        EXPECT_EQ ( 1 - n, lptr3[n] );
        EXPECT_EQ ( MEM_ALLOCATED_INUSE_WRITE, ptr.chunk->status );
        EXPECT_EQ ( MEM_ALLOCATED_INUSE_WRITE, ptr2.chunk->status );
        EXPECT_EQ ( MEM_ALLOCATED_INUSE_WRITE, ptr3.chunk->status );
    }
}


/**
 * @test Demonstrates how the application dies when a pointer is deleted while still in use
 */
TEST ( managedPtr, Unit_DeleteWhileInUse )
{
    /*
     * Idea: Death test, do we want to let everything die at this point?
     * Ideally, we are not allowed to throw in a destructor. Doing it anyways terminates programm
     */
    managedDummySwap swap ( 100 );
    //The following test will shoot its manager. To reenact the old one, lets save its pointer:
    managedMemory *old = managedMemory::defaultManager;

    cyclicManagedMemory *managedMemory = new cyclicManagedMemory ( &swap, 100 );
    managedPtr<double> *ptr = new managedPtr<double> ( 10 );
    ptr->setUse();

    ASSERT_DEATH ( delete ptr, "terminate called after throwing an instance of 'rambrain::memoryException'" );
    printf ( "Hello from below" );

    //We would like to call the destructor of managedMemory, however this destructor will segfault because of the death test...
    //Thus, only get the old one back again:
    managedMemory::defaultManager = old;
}


/**
 * @test Tests direct access on a managedPtr (deprecated since not thread safe and slow!)
 */
TEST ( managedPtr, Unit_DirectAccess )
{
    managedDummySwap swap ( 200 );
    cyclicManagedMemory managedMemory ( &swap, 200 );

    managedPtr<double> ptr ( 5 );
    for ( int n = 0; n < 5; n++ ) {
        ptr[n] = n;
    }

    for ( int n = 0; n < 5; n++ ) {
        EXPECT_EQ ( n, ptr[n] );
    }
}


/**
 * @test Tests direct access in a more complex scenario (depcrecated since not thread safe and slow!)
 */
TEST ( managedPtr, Unit_DirectAccessSwapped )
{
    const unsigned int alloc = 10u;
    const unsigned int memsize = 1.5 * alloc * sizeof ( double );
    const unsigned int swapsize = 10 * memsize;

    managedDummySwap swap ( swapsize );
    cyclicManagedMemory managedMemory ( &swap, memsize );

    managedPtr<double> ptr1 ( alloc );
    managedPtr<double> ptr2 ( alloc );
    for ( unsigned int n = 0; n < alloc; n++ ) {
        ptr1[n] = n;
        ptr2[n] = 2 * n;
    }

    for ( unsigned int n = 0; n < alloc; n++ ) {
        EXPECT_EQ ( n, ptr1[n] );
        EXPECT_EQ ( 2 * n, ptr2[n] );
    }
}


/**
 * @test Tests if classes can be managed properly
 */
TEST ( managedPtr, Unit_CreateAndInitialize )
{

    class nurfuerspassclass
    {
    public:
        nurfuerspassclass ( int a ) : val ( a ) {};
        int val;
    };

    const unsigned int alloc = 10u;
    const unsigned int memsize = 1.5 * alloc * sizeof ( double );
    const unsigned int swapsize = 10 * memsize;

    managedDummySwap swap ( swapsize );
    cyclicManagedMemory managedMemory ( &swap, memsize );

    managedPtr<nurfuerspassclass> havefun ( 5, 5 );
    adhereTo<nurfuerspassclass> hf ( havefun );
    nurfuerspassclass *hfbuf = hf;
    for ( int n = 0; n < 5; n++ ) {
        ASSERT_EQ ( 5, hfbuf[n].val );
    }

}


/**
 * @test Tests the different constructors of managedPtr
 */
TEST ( managedPtr, Unit_PointerAllocation )
{
    managedDummySwap swap ( 200 );
    cyclicManagedMemory managedMemory ( &swap, 200 );

    class A
    {
    public:
        A ( int a = 42 ) : a ( a ) {}
        int a;
    };

    managedPtr<A> myA1;
    ADHERETOLOC ( A, myA1, A1 );
    EXPECT_EQ ( 42, A1->a );

    managedPtr<A> myA2 ( 1 );
    ADHERETOLOC ( A, myA2, A2 );
    EXPECT_EQ ( 42, A2->a );

    managedPtr<A> myA3 ( 1, 43 );
    ADHERETOLOC ( A, myA3, A3 );
    EXPECT_EQ ( 43, A3->a );

    managedPtr<A> myA4 ( 2, 43 );
    ADHERETOLOC ( A, myA4, A4 );
    EXPECT_EQ ( 43, A4[0].a );
    EXPECT_EQ ( 43, A4[1].a );

}


/**
 * @test Tests if zero sized objects (while not usefull) can be handled
 */
TEST ( managedPtr, Unit_ZeroSizedObjects )
{
    managedDummySwap swap ( 200 );
    cyclicManagedMemory managedMemory ( &swap, 200 );
    managedPtr<bool> ptrnull ( 0 );
    managedPtr<double> ptr ( 5 );
    for ( int n = 0; n < 5; n++ ) {
        ptr[n] = n;
    }

    for ( int n = 0; n < 5; n++ ) {
        EXPECT_EQ ( n, ptr[n] );
    }
}


/**
 * @test Demonstrates why direct access if awfully slow in comparison to smart access via adhereto
 */
TEST ( managedPtr, Integration_DirectVsSmartAccess )
{
    const unsigned int alloc = 20000u;
    const unsigned int memsize = 1.5 * alloc * sizeof ( double );
    const unsigned int swapsize = 10 * memsize;
    const unsigned int runs = 10;
    tester test;
    test.startNewTimeCycle();

    managedDummySwap swap ( swapsize );
    cyclicManagedMemory managedMemory ( &swap, memsize );

    managedPtr<double> ptr1 ( alloc );
    {
        ADHERETOLOC ( double, ptr1, lptr1 );
        for ( unsigned int n = 0; n < alloc; n++ ) {
            lptr1[n] = n;
        }
    }
    managedPtr<double> ptr2 ( alloc );
    {
        ADHERETOLOC ( double, ptr2, lptr2 );
        for ( unsigned int n = 0; n < alloc; n++ ) {
            lptr2[n] = -n;
        }
    }

    test.addTimeMeasurement();

    {
        ADHERETOLOC ( double, ptr1, lptr1 );
        for ( unsigned int r = 0; r < runs; ++r ) {
            for ( unsigned int n = 0; n < alloc; n++ ) {
                EXPECT_EQ ( n, lptr1[n] );
            }
        }
    }
    {
        ADHERETOLOC ( double, ptr2, lptr2 );
        for ( unsigned int r = 0; r < runs; ++r ) {
            for ( unsigned int n = 0; n < alloc; n++ ) {
                EXPECT_EQ ( -n, lptr2[n] );
            }
        }
    }

    test.addTimeMeasurement();

    for ( unsigned int r = 0; r < runs; ++r ) {
        for ( unsigned int n = 0; n < alloc; n++ ) {
            EXPECT_EQ ( n, ptr1[n] );
            EXPECT_EQ ( -n, ptr2[n] );
        }
    }

    test.addTimeMeasurement();

    std::vector<int64_t> durations = test.getDurationsForCurrentCycle();
    infomsgf ( "Smart access ran for %ld ms", durations[0] );
    infomsgf ( "Direct access ran for %ld ms", durations[1] );
    infomsgf ( "Direct access cost %g as much time as smart access", 1.0 * durations[1] / durations[0] );
}


/**
 * @test Tests if parallel deleting of pointers work
 * @todo Why is there no assert / expect here??
 */
TEST ( managedPtr, Unit_MultithreadingConcurrentCreateDelete )
{
    managedDummySwap swap ( 2000 );
    cyclicManagedMemory managedMemory ( &swap, 2000 );

    const unsigned int arrsize = 200;

    managedPtr<double> *arr[arrsize];
    for ( unsigned int i = 0; i < arrsize; ++i ) {
        arr[i] = NULL;
    }
    #pragma omp parallel for
    for ( unsigned int i = 0; i < arrsize; ++i ) {
        arr[i] = new managedPtr<double> ( 1 );

    }
    #pragma omp parallel for
    for ( unsigned int i = 0; i < arrsize; ++i ) {
        delete arr[i];

    }
}


/**
 * @test Tests if concurrent access works properly while loading and unloading objects
 * @todo Why is there no assert / expect here???
 */
TEST ( managedPtr, Unit_ConcurrentUseAccess )
{
    managedDummySwap swap ( 800 );
    cyclicManagedMemory managedMemory ( &swap, 400 );

    managedMemory.setOutOfSwapIsFatal ( false ); //This may be needed by OMP programs

    managedPtr<double> a ( 40 );
    managedPtr<double> b ( 40 );

    #pragma omp parallel for
    for ( int i = 0; i < 1000; ++i ) {
        if ( i % 2 == 0 ) {
            ADHERETOLOC ( double, a, loc );
            loc[0] = i;

        } else {
            ADHERETOLOC ( double, b, loc );
            loc[0] = i;
        }
    }
}


/**
 * @test More complex scenario for concurrent access
 * @todo There must be more to assert...
 */
TEST ( managedPtr, Unit_ConcurrentUseAccessThree )
{
    managedDummySwap swap ( 1200 );
    cyclicManagedMemory managedMemory ( &swap, 400 );
    managedMemory.setOutOfSwapIsFatal ( false );
    managedPtr<double> a[] = {managedPtr<double> ( 40 ), managedPtr<double> ( 40 ), managedPtr<double> ( 40 ) };


    #pragma omp parallel for
    for ( int i = 0; i < 10000; ++i ) {
        if ( i % 500 == 0 ) { //Don't do this all the time as parallelism is blocked by this and this is what we want to test...
            EXPECT_TRUE ( managedMemory.checkCycle() );
        }
        int idx = i % 3;
        adhereTo<double> A ( a[idx] );
        double *loc = A;
        loc[0] = i;
    }
}


/**
 * @brief Helper class to track construction and destruction
 */
class destructorTracker
{
public:
    destructorTracker() {
        ++num_instances;    // Also do something in local memory to provoke SEGV if not correctly inited
        arr[9] = 9;
    }

    ~destructorTracker() {
        --num_instances;    // Also do something in local memory to provoke SEGV if not correctly loaded
        arr[9] = 0;
    }

    static int num_instances;
private:
    double arr[10];

};
int destructorTracker::num_instances = 0;


/**
 * @test Tests if class constructors and destructors are called appropriately as the hierarchy dictates
 */
TEST ( managedPtr, Unit_CallDestructorIfSwapped )
{
    managedDummySwap swap ( sizeof ( destructorTracker ) * 2 );
    cyclicManagedMemory managedMemory ( & swap, sizeof ( destructorTracker ) * 1.5 ) ;

    managedPtr<destructorTracker> *ptr1 = new managedPtr<destructorTracker> ( 1 );
    ASSERT_EQ ( 1, destructorTracker::num_instances );

    //Swap out first one:
    managedPtr<destructorTracker> *ptr2 = new managedPtr<destructorTracker> ( 1 );
    ASSERT_EQ ( 2, destructorTracker::num_instances );

    delete ptr2;
    ASSERT_EQ ( 1, destructorTracker::num_instances );

    delete ptr1;
    ASSERT_EQ ( 0, destructorTracker::num_instances );
}

RESTORE_WARNINGS;
