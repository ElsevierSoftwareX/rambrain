#include "managedMemory.h"
#include "managedPtr.h"
#include <gtest/gtest.h>

int main(int argc, char ** argv){
 ::testing::InitGoogleTest(&argc,argv);
 return RUN_ALL_TESTS();
};


TEST(BasicManagement,AllocatePointers){
  
  //Allocate Manager
  managedMemory manager(800);
  
  //Allocate 50% of space for an double array
  managedPtr<double> gPtr(50);
  
  //Try to rad from it
  adhereTo<double> gPtrI(gPtr);
  
  double * lPtr = gPtrI;
  
  ASSERT_TRUE(lPtr!=NULL);
  
  ASSERT_TRUE(manager.getUsedMemory()==sizeof(double)*50);
  
  for(int n=0;n<50;n++){
    lPtr[n] = 1.; //Should not segfault
  } 
}


class A{
public:
  managedPtr<double> testelements = managedPtr<double>(10);
};

TEST(BasicManagement,DeepAllocatePointers){
  managedMemory manager(800);
  
  managedPtr<A> managedA(1);
  
  adhereTo<A> adhA( managedA);
  A* locA = adhA;
  
  
  
};

