#include "nexa/memory_manager.hpp"
#include <iostream>
#define CHECK(expr) do { if (!(expr)) { std::cerr << "CHECK failed: " << #expr << " at line " << __LINE__ << '\n'; return 1; } } while (false)
#include <stdexcept>
#include <cstdio>
int main(){
 nexa::VirtualMemoryManager mm(1024);
 CHECK(mm.totalFrames()==4&&mm.freeFrames()==4);
 mm.createAddressSpace(1); mm.createAddressSpace(2);
 mm.mapPage(1,0); mm.mapPage(2,0);
 mm.write(1,0x0005,0x41); CHECK(mm.read(1,0x0005)==0x41);
 CHECK(mm.tlbMisses()>=1&&mm.tlbHits()>=1);
 const auto missesBeforeProtect=mm.tlbMisses();
 CHECK(mm.read(2,0x0005)==0);
 bool fault=false; try{(void)mm.read(1,0x0100);}catch(const std::runtime_error&){fault=true;} CHECK(fault);
 mm.protectPage(1,0,false,true);
 fault=false; try{mm.write(1,0,0x42);}catch(const std::runtime_error&){fault=true;} CHECK(fault);
 mm.protectPage(1,0,true,false);
 CHECK(mm.tlbMisses()>missesBeforeProtect);
 fault=false; try{(void)mm.read(1,0);}catch(const std::runtime_error&){fault=true;} CHECK(fault);
 CHECK(mm.read(1,5,nexa::MemoryAccess::Kernel)==0x41);
 mm.mapPage(1,1); mm.write(1,0x0100,0x77); CHECK(mm.read(1,0x0100)==0x77);
 mm.unmapPage(1,1); CHECK(mm.freeFrames()==2);
 mm.protectPage(2,0,true,true,false);
 fault=false; try{(void)mm.canExecute(2,0);}catch(const std::runtime_error&){fault=true;} CHECK(fault);
 const auto missesBeforeFlush=mm.tlbMisses(); mm.flushTlb(); (void)mm.read(1,5,nexa::MemoryAccess::Kernel); CHECK(mm.tlbMisses()==missesBeforeFlush+1);
 mm.destroyAddressSpace(1); CHECK(mm.freeFrames()==3);
 mm.destroyAddressSpace(2); CHECK(mm.freeFrames()==4);
 nexa::VirtualMemoryManager swapped(512);
 swapped.createAddressSpace(7);
 swapped.mapPage(7,0); swapped.mapPage(7,1);
 swapped.write(7,0,0xA0); swapped.write(7,256,0xB1);
 (void)swapped.read(7,0);
 swapped.mapPage(7,2);
 CHECK(swapped.read(7,512)==0);
 CHECK(swapped.swappedPages()==1&&swapped.swapOuts()==1);
 CHECK(swapped.read(7,256)==0xB1);
 CHECK(swapped.swapIns()==1);
 swapped.write(7,256,0xB2);
 CHECK(swapped.read(7,256)==0xB2);
 swapped.destroyAddressSpace(7);
 CHECK(swapped.freeFrames()==swapped.totalFrames());

 nexa::VirtualMemoryManager recovery(512);
 recovery.createAddressSpace(42);
 const auto pinnedA=recovery.createSharedFrame();
 const auto pinnedB=recovery.createSharedFrame();
 int recoveries=0;
 recovery.setAllocationRecoveryHandler([&]{++recoveries; recovery.releaseSharedFrame(pinnedB);});
 recovery.mapPage(42,0);
 CHECK(recoveries==1&&recovery.freeFrames()==0);
 CHECK(recovery.allocationRecoveryAttempts()==1&&recovery.allocationRecoverySuccesses()==1&&recovery.allocationRecoveryFailures()==0);
 CHECK(recovery.read(42,0)==0);
 recovery.destroyAddressSpace(42);
 recovery.releaseSharedFrame(pinnedA);
 CHECK(recovery.freeFrames()==recovery.totalFrames());
 nexa::VirtualMemoryManager failedRecovery(512);
 failedRecovery.createAddressSpace(43);
 const auto failedPinA=failedRecovery.createSharedFrame();
 const auto failedPinB=failedRecovery.createSharedFrame();
 failedRecovery.setAllocationRecoveryHandler([]{});
 bool allocationFailed=false;
 try{failedRecovery.mapPage(43,0);}catch(const std::runtime_error&){allocationFailed=true;}
 CHECK(allocationFailed&&failedRecovery.allocationRecoveryAttempts()==1&&failedRecovery.allocationRecoveryFailures()==1&&failedRecovery.allocationRecoverySuccesses()==0);
 failedRecovery.releaseSharedFrame(failedPinA); failedRecovery.releaseSharedFrame(failedPinB); failedRecovery.destroyAddressSpace(43);
 CHECK(failedRecovery.freeFrames()==failedRecovery.totalFrames());

 nexa::VirtualMemoryManager diskSwap(512);
 diskSwap.setSwapFile("nexa_swap_test.bin");
 diskSwap.createAddressSpace(9);
 diskSwap.mapPage(9,0); diskSwap.mapPage(9,1);
 diskSwap.write(9,0,0xC3); diskSwap.write(9,256,0xD4);
 (void)diskSwap.read(9,0);
 diskSwap.mapPage(9,2);
 CHECK(diskSwap.swappedPages()==1);
 CHECK(diskSwap.read(9,256)==0xD4);
 CHECK(diskSwap.swapIns()==1&&diskSwap.swapOuts()>=2);
 diskSwap.destroyAddressSpace(9);
 CHECK(diskSwap.freeFrames()==diskSwap.totalFrames());
 std::remove("nexa_swap_test.bin");

}
