#include "nexa/executable.hpp"
#include "nexa/vfs.hpp"
#include "nexa/assembler.hpp"
#include "nexa/vm.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>
int main(){
 nexa::Executable e{0,{{nexa::Opcode::LoadImmediate,42},{nexa::Opcode::Halt,0}}};const char*p="nexa_test.exe";nexa::saveExecutable(e,p);auto q=nexa::loadExecutable(p);assert(q.entryPoint==0&&q.code.size()==2&&q.code[0].operand==42);std::remove(p);
 nexa::VM vm;vm.loadProgram(nexa::parseAssembly("INT 1\nHALT\nLOAD A, 99\nIRET\n"));vm.setInterruptVector(1,2);vm.stepInstruction();assert(vm.programCounter()==2);vm.run();assert(vm.halted()&&vm.accumulator()==99);
 nexa::VirtualFileSystem fs;fs.createDirectory("/bin");fs.createFile("/bin/demo",{1,2,3});assert(fs.readFile("/bin/demo").size()==3);assert(fs.list("/bin").at(0)=="demo");fs.writeFile("/bin/demo",{9});assert(fs.readFile("/bin/demo")[0]==9);fs.removeFile("/bin/demo");assert(!fs.exists("/bin/demo"));bool blocked=false;try{fs.createFile("/../escape");}catch(const std::invalid_argument&){blocked=true;}assert(blocked);
}
