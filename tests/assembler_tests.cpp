#include "nexa/assembler.hpp"
#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>\n#include <cstdio>
int main(){
 const auto p=nexa::parseAssembly("LOAD A, 0x10\nLOAD B, 2\nADD A, B\nSTORE [5]\nLOAD A, [5]\nPRINT A\nHALT\n");
 assert(p.size()==7);assert(p[0].opcode==nexa::Opcode::LoadImmediate&&p[0].operand==16);assert(p[1].opcode==nexa::Opcode::LoadBImmediate&&p[1].operand==2);assert(p[2].opcode==nexa::Opcode::Add);assert(p[3].opcode==nexa::Opcode::Store&&p[3].operand==5);assert(p[4].opcode==nexa::Opcode::Load&&p[4].operand==5);assert(p[5].opcode==nexa::Opcode::Print);assert(p[6].opcode==nexa::Opcode::Halt);
 bool threw=false;try{(void)nexa::parseAssembly("BOGUS\n");}catch(const std::runtime_error&){threw=true;}assert(threw);
 threw=false;try{(void)nexa::parseAssembly("LOAD A, 999\n");}catch(const std::runtime_error&){threw=true;}assert(threw);
 threw=false;try{(void)nexa::parseAssembly("LOAD A,\n");}catch(const std::runtime_error&){threw=true;}assert(threw);
 const auto wide=nexa::parseAssembly("LOAD A, 77\nSTORE [300]\nLOAD A, [300]\nHALT\n");nexa::VM vm(1024);vm.loadProgram(wide);vm.run();assert(vm.readMemory(300)==77);assert(vm.accumulator()==77);assert(vm.instructionsExecuted()==4);
 threw=false;try{(void)nexa::parseAssembly("STORE [65536]\nHALT\n");}catch(const std::runtime_error&){threw=true;}assert(threw);
 // Branching countdown loop: print 3,2,1
 auto loop=nexa::parseAssembly("LOAD A, 3\nagain:\nPRINT A\nDEC A\nCMP A, 0\nJNZ again\nHALT\n");assert(loop[4].opcode==nexa::Opcode::JumpNotZero&&loop[4].operand==1);
 std::ostringstream capture;auto*old=std::cout.rdbuf(capture.rdbuf());nexa::VM vm2;vm2.loadProgram(loop);vm2.run();std::cout.rdbuf(old);assert(capture.str()=="Output: 3\nOutput: 2\nOutput: 1\n");
 auto call=nexa::parseAssembly("LOAD A, 9\nCALL fn\nPRINT A\nHALT\nfn:\nINC A\nRET\n");capture.str("");old=std::cout.rdbuf(capture.rdbuf());vm2.loadProgram(call);vm2.run();std::cout.rdbuf(old);assert(capture.str()=="Output: 10\n");
 auto stack=nexa::parseAssembly("LOAD A, 8\nPUSH A\nLOAD A, 1\nPOP A\nPRINT A\nHALT\n");capture.str("");old=std::cout.rdbuf(capture.rdbuf());vm2.loadProgram(stack);vm2.run();std::cout.rdbuf(old);assert(capture.str()=="Output: 8\n");
 vm2.loadProgram(call);vm2.saveState("nexa_test.state");nexa::VM restored;restored.loadState("nexa_test.state");capture.str("");old=std::cout.rdbuf(capture.rdbuf());restored.run();std::cout.rdbuf(old);assert(capture.str()=="Output: 10\n");assert(restored.accumulator()==10);std::remove("nexa_test.state");
 auto infinite=nexa::parseAssembly("forever:\nJMP forever\n");restored.loadProgram(infinite);threw=false;try{restored.run(8);}catch(const std::runtime_error&){threw=true;}assert(threw);
}