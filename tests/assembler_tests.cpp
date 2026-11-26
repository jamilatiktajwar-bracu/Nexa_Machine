#include "nexa/assembler.hpp"
#include <cassert>
#include <stdexcept>
int main(){
 const auto p=nexa::parseAssembly("LOAD A, 0x10\nLOAD B, 2\nADD A, B\nSTORE [5]\nLOAD A, [5]\nPRINT A\nHALT\n");
 assert(p.size()==7);assert(p[0].opcode==nexa::Opcode::LoadImmediate&&p[0].operand==16);assert(p[1].opcode==nexa::Opcode::LoadBImmediate&&p[1].operand==2);assert(p[2].opcode==nexa::Opcode::Add);assert(p[3].opcode==nexa::Opcode::Store&&p[3].operand==5);assert(p[4].opcode==nexa::Opcode::Load&&p[4].operand==5);assert(p[5].opcode==nexa::Opcode::Print);assert(p[6].opcode==nexa::Opcode::Halt);
 bool threw=false;try{(void)nexa::parseAssembly("BOGUS\n");}catch(const std::runtime_error&){threw=true;}assert(threw);
 threw=false;try{(void)nexa::parseAssembly("LOAD A, 999\n");}catch(const std::runtime_error&){threw=true;}assert(threw);
 threw=false;try{(void)nexa::parseAssembly("LOAD A,\n");}catch(const std::runtime_error&){threw=true;}assert(threw);
 const auto wide=nexa::parseAssembly("LOAD A, 77\nSTORE [300]\nLOAD A, [300]\nHALT\n");nexa::VM vm(1024);vm.loadProgram(wide);vm.run();assert(vm.readMemory(300)==77);assert(vm.accumulator()==77);assert(vm.instructionsExecuted()==4);
 threw=false;try{(void)nexa::parseAssembly("STORE [65536]\nHALT\n");}catch(const std::runtime_error&){threw=true;}assert(threw);
}
