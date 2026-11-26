#include "nexa/vm.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
namespace nexa {
VM::VM(std::size_t n):ram_(n,0),framebuffer_(ScreenWidth*ScreenHeight,0){if(!n||n>65536)throw std::invalid_argument("RAM must be 1..65536 bytes");}
void VM::loadProgram(std::vector<Instruction>p){program_=std::move(p);reset();}
void VM::setProgramCounter(std::uint16_t a){if(a>=program_.size())throw std::out_of_range("Program entry outside code");pc_=a;}
void VM::reset(){pc_=0;a_=b_=0;std::fill(ram_.begin(),ram_.end(),0);stack_.clear();keyboard_.clear();deviceOutput_.clear();std::fill(framebuffer_.begin(),framebuffer_.end(),0);graphicsX_=graphicsY_=0;graphicsColor_=1;watchpointHit_=false;halted_=false;zeroFlag_=false;privilegeMode_=PrivilegeMode::User;instructionCount_=0;}
std::uint8_t VM::readMemory(std::uint16_t a)const{if(a>=ram_.size())return 0;return memoryReader_?memoryReader_(a):ram_[a];}
void VM::writeMemory(std::uint16_t a,std::uint8_t v){if(a>=ram_.size())throw std::out_of_range("RAM address out of range");if(memoryWriter_)memoryWriter_(a,v);else ram_[a]=v;if(watchpointEnabled_&&a==watchedAddress_)watchpointHit_=true;}
void VM::setWatchpoint(std::uint16_t a){if(a>=ram_.size())throw std::out_of_range("Watchpoint address outside RAM");watchedAddress_=a;watchpointEnabled_=true;watchpointHit_=false;}
void VM::clearWatchpoint(std::uint16_t a){if(watchpointEnabled_&&watchedAddress_==a)watchpointEnabled_=false;watchpointHit_=false;}
bool VM::pixel(std::size_t x,std::size_t y)const noexcept{return x<ScreenWidth&&y<ScreenHeight&&framebuffer_[y*ScreenWidth+x]!=0;}
void VM::drawPixel(std::size_t x,std::size_t y,bool white)noexcept{if(x<ScreenWidth&&y<ScreenHeight)framebuffer_[y*ScreenWidth+x]=white?1:0;}
void VM::run(std::size_t budget){std::size_t start=instructionCount_;while(!halted_){if(hasBreakpoint(pc_)||watchpointHit_)return;if(instructionCount_-start>=budget)throw std::runtime_error("Instruction budget exceeded; possible infinite loop");step();}}
bool VM::stepInstruction(){if(halted_)return false;step();return !halted_;}
void VM::queueKeyboardInput(const std::string& text){for(unsigned char ch:text)keyboard_.push_back(ch);}
void VM::setBreakpoint(std::size_t a){breakpoints_.insert(a);}
void VM::clearBreakpoint(std::size_t a){breakpoints_.erase(a);}
void VM::clearBreakpoints(){breakpoints_.clear();}
bool VM::hasBreakpoint(std::size_t a)const{return breakpoints_.contains(a);}
void VM::setInterruptVector(std::uint8_t v,std::uint16_t a){if(a>=program_.size())throw std::out_of_range("Interrupt handler outside program");interruptVectors_[v]=a;}
void VM::triggerInterrupt(std::uint8_t v){auto it=interruptVectors_.find(v);if(it==interruptVectors_.end())throw std::runtime_error("Unhandled interrupt vector");stack_.push_back(static_cast<std::uint16_t>(pc_));pc_=it->second;halted_=false;}
void VM::saveState(const std::string&path)const{
 std::ofstream out(path,std::ios::binary);if(!out)throw std::runtime_error("Cannot write state file: "+path);
 const char magic[8]={'N','E','X','A','S','T','0','3'};out.write(magic,8);
 auto put=[&](auto v){out.write(reinterpret_cast<const char*>(&v),sizeof(v));};
 std::uint32_t ram=static_cast<std::uint32_t>(ram_.size()),prog=static_cast<std::uint32_t>(program_.size()),stk=static_cast<std::uint32_t>(stack_.size());
 std::uint64_t savedPc=pc_,savedCount=instructionCount_;std::uint8_t savedHalted=halted_?1:0,savedZero=zeroFlag_?1:0;
 put(ram);put(prog);put(stk);put(a_);put(b_);put(savedPc);put(savedCount);put(savedHalted);put(savedZero);
 out.write(reinterpret_cast<const char*>(ram_.data()),static_cast<std::streamsize>(ram_.size()));out.write(reinterpret_cast<const char*>(framebuffer_.data()),static_cast<std::streamsize>(framebuffer_.size()));
 for(auto i:program_){put(i.opcode);put(i.operand);}
 for(auto s:stack_)put(s);
 if(!out)throw std::runtime_error("Failed writing state");
}
void VM::loadState(const std::string&path){
 std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot open state file: "+path);
 char magic[8];in.read(magic,8);if(!in||std::string(magic,8)!="NEXAST03")throw std::runtime_error("Invalid Nexa state file");
 auto get=[&](auto&v){in.read(reinterpret_cast<char*>(&v),sizeof(v));if(!in)throw std::runtime_error("Truncated state file");};
 std::uint32_t ram=0,prog=0,stk=0;std::uint8_t na=0,nb=0,nh=0,nz=0;std::uint64_t npc=0,ncount=0;
 get(ram);get(prog);get(stk);get(na);get(nb);get(npc);get(ncount);get(nh);get(nz);
 if(ram==0||ram>65536||prog>65535||stk>1000000||npc>prog||nh>1||nz>1)throw std::runtime_error("Invalid state metadata");
 std::vector<std::uint8_t> nr(ram);in.read(reinterpret_cast<char*>(nr.data()),static_cast<std::streamsize>(nr.size()));if(!in)throw std::runtime_error("Truncated RAM in state");
 std::vector<std::uint8_t> nfb(ScreenWidth*ScreenHeight);in.read(reinterpret_cast<char*>(nfb.data()),static_cast<std::streamsize>(nfb.size()));if(!in)throw std::runtime_error("Truncated framebuffer");std::vector<Instruction> np;np.reserve(prog);
 for(std::uint32_t k=0;k<prog;++k){Instruction ins;get(ins.opcode);get(ins.operand);if(static_cast<unsigned>(ins.opcode)>static_cast<unsigned>(Opcode::Halt))throw std::runtime_error("Invalid opcode in state");np.push_back(ins);}
 std::vector<std::uint16_t> ns;ns.reserve(stk);for(std::uint32_t k=0;k<stk;++k){std::uint16_t val;get(val);ns.push_back(val);}
 char extra;if(in.read(&extra,1))throw std::runtime_error("Unexpected trailing state data");
 ram_=std::move(nr);framebuffer_=std::move(nfb);program_=std::move(np);stack_=std::move(ns);a_=na;b_=nb;pc_=static_cast<std::size_t>(npc);instructionCount_=static_cast<std::size_t>(ncount);halted_=nh!=0;zeroFlag_=nz!=0;
}
void VM::step(){
 if(pc_>=program_.size())throw std::runtime_error("Program counter beyond program; missing HALT?");
 if(instructionFetchGuard_){if(pc_>65535)throw std::runtime_error("Instruction address exceeds virtual address space");instructionFetchGuard_(static_cast<std::uint16_t>(pc_));}
 auto i=instructionReader_?instructionReader_(static_cast<std::uint16_t>(pc_)):program_[pc_];++pc_;++instructionCount_;
 switch(i.opcode){
 case Opcode::LoadImmediate:if(i.operand>255)throw std::runtime_error("Immediate overflow");a_=static_cast<std::uint8_t>(i.operand);zeroFlag_=a_==0;break;
 case Opcode::LoadBImmediate:if(i.operand>255)throw std::runtime_error("Immediate overflow");b_=static_cast<std::uint8_t>(i.operand);zeroFlag_=b_==0;break;
 case Opcode::Add:a_=static_cast<std::uint8_t>(a_+b_);zeroFlag_=a_==0;break;
 case Opcode::Sub:a_=static_cast<std::uint8_t>(a_-b_);zeroFlag_=a_==0;break;
 case Opcode::Inc:a_=static_cast<std::uint8_t>(a_+1);zeroFlag_=a_==0;break;
 case Opcode::Dec:a_=static_cast<std::uint8_t>(a_-1);zeroFlag_=a_==0;break;
 case Opcode::Compare:zeroFlag_=a_==b_;break;
 case Opcode::CompareImmediate:if(i.operand>255)throw std::runtime_error("Immediate overflow");zeroFlag_=a_==i.operand;break;
 case Opcode::Store:if(i.operand>=ram_.size())throw std::runtime_error("RAM address out of range");writeMemory(i.operand,a_);break;
 case Opcode::Load:if(i.operand>=ram_.size())throw std::runtime_error("RAM address out of range");a_=readMemory(i.operand);zeroFlag_=a_==0;break;
 case Opcode::Print:std::cout<<"Output: "<<static_cast<unsigned>(a_)<<'\n';break;
 case Opcode::Input:if(i.operand==KeyboardPort){a_=keyboard_.empty()?0:keyboard_.front();if(!keyboard_.empty())keyboard_.pop_front();zeroFlag_=a_==0;}else if(i.operand==GraphicsXPort)a_=graphicsX_;else if(i.operand==GraphicsYPort)a_=graphicsY_;else throw std::runtime_error("Unknown input port");break;
 case Opcode::Output:if(i.operand==DisplayPort){deviceOutput_.push_back(static_cast<char>(a_));std::cout.put(static_cast<char>(a_));}else if(i.operand==GraphicsXPort)graphicsX_=a_;else if(i.operand==GraphicsYPort)graphicsY_=a_;else if(i.operand==GraphicsColorPort)graphicsColor_=a_?1:0;else if(i.operand==GraphicsDrawPort){if(graphicsX_<ScreenWidth&&graphicsY_<ScreenHeight)framebuffer_[graphicsY_*ScreenWidth+graphicsX_]=graphicsColor_;}else if(i.operand==GraphicsClearPort)std::fill(framebuffer_.begin(),framebuffer_.end(),a_?1:0);else throw std::runtime_error("Unknown output port");break;
 case Opcode::Jump:pc_=i.operand;break;
 case Opcode::JumpZero:if(zeroFlag_)pc_=i.operand;break;
 case Opcode::JumpNotZero:if(!zeroFlag_)pc_=i.operand;break;
 case Opcode::PushA:stack_.push_back(a_);break;
 case Opcode::PushB:stack_.push_back(b_);break;
 case Opcode::PopA:if(stack_.empty())throw std::runtime_error("Stack underflow");a_=static_cast<std::uint8_t>(stack_.back());stack_.pop_back();zeroFlag_=a_==0;break;
 case Opcode::PopB:if(stack_.empty())throw std::runtime_error("Stack underflow");b_=static_cast<std::uint8_t>(stack_.back());stack_.pop_back();zeroFlag_=b_==0;break;
 case Opcode::Call:stack_.push_back(static_cast<std::uint16_t>(pc_));pc_=i.operand;break;
 case Opcode::Return:if(stack_.empty())throw std::runtime_error("Stack underflow on RET");pc_=stack_.back();stack_.pop_back();break;
 case Opcode::Interrupt:triggerInterrupt(static_cast<std::uint8_t>(i.operand));break;
 case Opcode::InterruptReturn:if(stack_.empty())throw std::runtime_error("Stack underflow on IRET");pc_=stack_.back();stack_.pop_back();break;
 case Opcode::Syscall:if(syscallHandler_){auto service=a_;const auto previousMode=privilegeMode_;privilegeMode_=PrivilegeMode::Kernel;try{a_=syscallHandler_(service,b_);}catch(...){privilegeMode_=previousMode;throw;}privilegeMode_=previousMode;zeroFlag_=a_==0;if(service==0)halted_=true;}else switch(a_){case 0:halted_=true;break;case 1:deviceOutput_.push_back(static_cast<char>(b_));std::cout.put(static_cast<char>(b_));break;case 2:a_=keyboard_.empty()?0:keyboard_.front();if(!keyboard_.empty())keyboard_.pop_front();zeroFlag_=a_==0;break;case 3:std::fill(framebuffer_.begin(),framebuffer_.end(),0);break;default:throw std::runtime_error("Unknown syscall service (A=service)");}break;
 case Opcode::Halt:halted_=true;break;
 default:throw std::runtime_error("Unknown opcode");
 }
}
}
