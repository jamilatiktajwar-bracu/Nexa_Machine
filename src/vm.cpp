#include "nexa/vm.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>\n#include <fstream>\n#include <string>
namespace nexa {
VM::VM(std::size_t n):ram_(n,0){if(!n||n>65536)throw std::invalid_argument("RAM must be 1..65536 bytes");}
void VM::loadProgram(std::vector<Instruction>p){program_=std::move(p);reset();}
void VM::reset(){pc_=0;a_=b_=0;std::fill(ram_.begin(),ram_.end(),0);stack_.clear();halted_=false;zeroFlag_=false;instructionCount_=0;}
std::uint8_t VM::readMemory(std::uint16_t a)const noexcept{return a<ram_.size()?ram_[a]:0;}
void VM::run(std::size_t budget){std::size_t start=instructionCount_;while(!halted_){if(instructionCount_-start>=budget)throw std::runtime_error(\"Instruction budget exceeded; possible infinite loop\");step();}}\nvoid VM::saveState(const std::string&path)const{\n std::ofstream out(path,std::ios::binary);if(!out)throw std::runtime_error(\"Cannot write state file: \"+path);\n const char magic[8]={'N','E','X','A','S','T','0','1'};out.write(magic,8);\n auto put=[&](auto v){out.write(reinterpret_cast<const char*>(&v),sizeof(v));};\n std::uint32_t ram=static_cast<std::uint32_t>(ram_.size()),prog=static_cast<std::uint32_t>(program_.size()),stk=static_cast<std::uint32_t>(stack_.size());\n put(ram);put(prog);put(stk);put(a_);put(b_);put(pc_);put(instructionCount_);put(halted_);put(zeroFlag_);\n out.write(reinterpret_cast<const char*>(ram_.data()),ram_.size());for(auto i:program_){put(i.opcode);put(i.operand);}for(auto s:stack_)put(s);if(!out)throw std::runtime_error(\"Failed writing state\");\n}\nvoid VM::loadState(const std::string&path){\n std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error(\"Cannot open state file: \"+path);char magic[8];in.read(magic,8);if(!in||std::string(magic,8)!=\"NEXAST01\")throw std::runtime_error(\"Invalid Nexa state file\");\n auto get=[&](auto&v){in.read(reinterpret_cast<char*>(&v),sizeof(v));if(!in)throw std::runtime_error(\"Truncated state file\");};\n std::uint32_t ram=0,prog=0,stk=0;std::uint8_t na=0,nb=0;std::size_t npc=0,ncount=0;bool nh=false,nz=false;get(ram);get(prog);get(stk);get(na);get(nb);get(npc);get(ncount);get(nh);get(nz);\n if(ram==0||ram>65536||prog>65535||stk>1000000||npc>prog)throw std::runtime_error(\"Invalid state metadata\");\n std::vector<std::uint8_t> nr(ram);in.read(reinterpret_cast<char*>(nr.data()),nr.size());if(!in)throw std::runtime_error(\"Truncated RAM in state\");std::vector<Instruction> np;np.reserve(prog);for(std::uint32_t k=0;k<prog;++k){Instruction ins;get(ins.opcode);get(ins.operand);if(static_cast<unsigned>(ins.opcode)>static_cast<unsigned>(Opcode::Halt))throw std::runtime_error(\"Invalid opcode in state\");np.push_back(ins);}std::vector<std::uint16_t> ns;ns.reserve(stk);for(std::uint32_t k=0;k<stk;++k){std::uint16_t val;get(val);ns.push_back(val);}char extra;if(in.read(&extra,1))throw std::runtime_error(\"Unexpected trailing state data\");\n ram_=std::move(nr);program_=std::move(np);stack_=std::move(ns);a_=na;b_=nb;pc_=npc;instructionCount_=ncount;halted_=nh;zeroFlag_=nz;\n}
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