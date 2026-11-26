#include "nexa/vm.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>
namespace nexa {
VM::VM(std::size_t n):ram_(n,0){if(!n||n>65536)throw std::invalid_argument("RAM must be 1..65536 bytes");}
void VM::loadProgram(std::vector<Instruction>p){program_=std::move(p);reset();}
void VM::reset(){pc_=0;a_=b_=0;std::fill(ram_.begin(),ram_.end(),0);halted_=false;instructionCount_=0;}
std::uint8_t VM::readMemory(std::uint16_t a)const noexcept{return a<ram_.size()?ram_[a]:0;}
void VM::run(){while(!halted_)step();}
void VM::step(){if(pc_>=program_.size())throw std::runtime_error("Program counter beyond program; missing HALT?");auto i=program_[pc_++];++instructionCount_;switch(i.opcode){case Opcode::LoadImmediate:if(i.operand>255)throw std::runtime_error("Immediate overflow");a_=static_cast<std::uint8_t>(i.operand);break;case Opcode::LoadBImmediate:if(i.operand>255)throw std::runtime_error("Immediate overflow");b_=static_cast<std::uint8_t>(i.operand);break;case Opcode::Add:a_=static_cast<std::uint8_t>(a_+b_);break;case Opcode::Store:if(i.operand>=ram_.size())throw std::runtime_error("RAM address out of range");ram_[i.operand]=a_;break;case Opcode::Load:if(i.operand>=ram_.size())throw std::runtime_error("RAM address out of range");a_=ram_[i.operand];break;case Opcode::Print:std::cout<<"Output: "<<static_cast<unsigned>(a_)<<'\n';break;case Opcode::Halt:halted_=true;break;default:throw std::runtime_error("Unknown opcode");}}
}