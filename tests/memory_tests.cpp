#include "nexa/memory_manager.hpp"
#include <cassert>
#include <stdexcept>
int main(){
 nexa::VirtualMemoryManager mm(1024);
 assert(mm.totalFrames()==4&&mm.freeFrames()==4);
 mm.createAddressSpace(1); mm.createAddressSpace(2);
 mm.mapPage(1,0); mm.mapPage(2,0);
 mm.write(1,0x0005,0x41); assert(mm.read(1,0x0005)==0x41);
 assert(mm.read(2,0x0005)==0);
 bool fault=false; try{(void)mm.read(1,0x0100);}catch(const std::runtime_error&){fault=true;} assert(fault);
 mm.protectPage(1,0,false,true);
 fault=false; try{mm.write(1,0,0x42);}catch(const std::runtime_error&){fault=true;} assert(fault);
 mm.protectPage(1,0,true,false);
 fault=false; try{(void)mm.read(1,0);}catch(const std::runtime_error&){fault=true;} assert(fault);
 assert(mm.read(1,0,nexa::MemoryAccess::Kernel)==0x41);
 mm.mapPage(1,1); mm.write(1,0x0100,0x77); assert(mm.read(1,0x0100)==0x77);
 mm.unmapPage(1,1); assert(mm.freeFrames()==2);
 mm.protectPage(2,0,true,true,false);
 fault=false; try{(void)mm.canExecute(2,0);}catch(const std::runtime_error&){fault=true;} assert(fault);
 mm.destroyAddressSpace(1); assert(mm.freeFrames()==3);
 mm.destroyAddressSpace(2); assert(mm.freeFrames()==4);
}
