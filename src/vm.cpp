#include "nexa/vm.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>
namespace nexa {
VM::VM(std::size_t n):ram_(n,0){if(!n||n>65536)throw std::invalid_argument("RAM must be 1..65536 bytes");}
void VM::loadProgram(std::vector<Instruction>p){program_=std::move(p);reset();}
void VM::reset(){pc_=0;a_=b_=0;std::fill(ram_.begin(),ram_.end(),0);stack_.clear();halted_=false;zeroFlag_=false;instructionCount_=0;}
std::uint8_t VM::readMemory(std::uint16_t a)const noexcept{return a<ram_.size()?ram_[a]:0;}
void VM::run(){while(!halted_)step();}
void VM::step(){
 if(pc_>=program_.size())throw std::runtime_error("Program counter beyond program; missing HALT?");
 auto i=program_[pc_++];++instructionCount_;
 switch(i.opcode){
 case Opcode::LoadImmediate:if(i.operand>255)throw std::runtime_error("Immediate overflow");a_=static_cast<std::uint8_t>(i.operand);zeroFlag_=a_==0;break;
 case Opcode::LoadBImmediate:if(i.operand>255)throw std::runtime_error("Immediate overflow");b_=static_cast<std::uint8_t>(i.operand);zeroFlag_=b_==0;break;
 case Opcode::Add:a_=static_cast<std::uint8_t>(a_+b_);zeroFlag_=a_==0;break;
 case Opcode::Sub:a_=static_cast<std::uint8_t>(a_-b_);zeroFlag_=a_==0;break;
 case Opcode::Inc:a_=static_cast<std::uint8_t>(a_+1);zeroFlag_=a_==0;break;
 case Opcode::Dec:a_=static_cast<std::uint8_t>(a_-1);zeroFlag_=a_==0;break;
 case Opcode::Compare:zeroFlag_=a_==b_;break;
 case Opcode::CompareImmediate:if(i.operand>255)throw std::runtime_error("Immediate overflow");zeroFlag_=a_==i.operand;break;
 case Opcode::Store:if(i.operand>=ram_.size())throw std::runtime_error("RAM address out of range");ram_[i.operand]=a_;break;
 case Opcode::Load:if(i.operand>=ram_.size())throw std::runtime_error("RAM address out of range");a_=ram_[i.operand];zeroFlag_=a_==0;break;
 case Opcode::Print:std::cout<<"Output: "<<static_cast<unsigned>(a_)<<'\n';break;
 case Opcode::Jump:pc_=i.operand;break;
 case Opcode::JumpZero:if(zeroFlag_)pc_=i.operand;break;
 case Opcode::JumpNotZero:if(!zeroFlag_)pc_=i.operand;break;
 case Opcode::PushA:stack_.push_back(a_);break;
 case Opcode::PushB:stack_.push_back(b_);break;
 case Opcode::PopA:if(stack_.empty())throw std::runtime_error("Stack underflow");a_=static_cast<std::uint8_t>(stack_.back());stack_.pop_back();zeroFlag_=a_==0;break;
 case Opcode::PopB:if(stack_.empty())throw std::runtime_error("Stack underflow");b_=static_cast<std::uint8_t>(stack_.back());stack_.pop_back();zeroFlag_=b_==0;break;
 case Opcode::Call:stack_.push_back(static_cast<std::uint16_t>(pc_));pc_=i.operand;break;
 case Opcode::Return:if(stack_.empty())throw std::runtime_error("Stack underflow on RET");pc_=stack_.back();stack_.pop_back();break;
 case Opcode::Halt:halted_=true;break;
 default:throw std::runtime_error("Unknown opcode");
 }
}
}