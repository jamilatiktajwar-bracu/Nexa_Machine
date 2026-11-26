#include "nexa/config.hpp"
#include "nexa/virtual_disk.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>
int main(){
 const char* cfg="nexa_test_config.json"; const char* disk="nexa_test.disk";
 nexa::MachineConfig c; c.name="TestVM";c.ramBytes=512;c.diskPath=disk;c.diskBytes=128;c.save(cfg);
 auto loaded=nexa::MachineConfig::load(cfg);assert(loaded.name=="TestVM");assert(loaded.ramBytes==512);assert(loaded.diskBytes==128);
 nexa::VirtualDisk d(disk,128);d.write(5,{1,2,3,4});assert((d.read(5,4)==std::vector<unsigned char>{1,2,3,4}));d.flush();
 nexa::VirtualDisk d2(disk,128);d2.load();assert((d2.read(5,4)==std::vector<unsigned char>{1,2,3,4}));
 bool threw=false;try{d2.read(127,2);}catch(const std::out_of_range&){threw=true;}assert(threw);
 std::remove(cfg);std::remove(disk);
}
