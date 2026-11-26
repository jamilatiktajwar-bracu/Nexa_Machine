#include "nexa/assembler.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
namespace nexa {namespace {
std::string trim(std::string s){auto a=s.find_first_not_of(" \t\r\n");if(a==std::string::npos)return {};auto b=s.find_last_not_of(" \t\r\n");return s.substr(a,b-a+1);}
std::string upper(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});return s;}
std::vector<std::string>split(const std::string&s){std::stringstream ss(s);std::vector<std::string>v;std::string p;while(std::getline(ss,p,','))v.push_back(trim(p));return v;}
std::uint16_t number(std::string s,std::size_t line,std::uint32_t max){s=trim(s);try{std::size_t used=0;auto v=std::stoul(s,&used,0);if(used!=s.size()||v>max)throw std::runtime_error("range");return static_cast<std::uint16_t>(v);}catch(...){throw std::runtime_error("Assembly line "+std::to_string(line)+": invalid number '"+s+"'");}}
std::string unbracket(std::string s){s=trim(s);if(s.size()>=2&&s.front()=='['&&s.back()==']')return trim(s.substr(1,s.size()-2));return s;}
struct Line{std::size_t n;std::string text;};
bool isLabel(const std::string&s){return !s.empty()&&s.back()==':'&&s.find_first_of(" \t")==std::string::npos;}
}
std::vector<Instruction>parseAssembly(const std::string&src){
 std::vector<Line>lines;std::unordered_map<std::string,std::uint16_t>labels,constants;std::stringstream in(src);std::string line;std::size_t n=0,pc=0;
 while(std::getline(in,line)){++n;auto c=line.find_first_of("#;");if(c!=std::string::npos)line.erase(c);auto slash=line.find("//");if(slash!=std::string::npos)line.erase(slash);line=trim(line);if(line.empty())continue;
 if(line.rfind("CONST ",0)==0||line.rfind(".CONST ",0)==0){auto def=split(trim(line.substr(line[0]=='.'?6:5)));if(def.size()!=2)throw std::runtime_error("Assembly line "+std::to_string(n)+": CONST syntax is CONST NAME, value");auto name=upper(def[0]);if(labels.contains(name)||constants.contains(name))throw std::runtime_error("Assembly line "+std::to_string(n)+": duplicate symbol "+name);constants[name]=number(def[1],n,65535);continue;}
 if(isLabel(line)){auto name=upper(line.substr(0,line.size()-1));if(labels.contains(name))throw std::runtime_error("Assembly line "+std::to_string(n)+": duplicate label "+name);if(pc>65535)throw std::runtime_error("Program too large");labels[name]=static_cast<std::uint16_t>(pc);continue;}
 lines.push_back({n,line});++pc;
 }
 std::vector<Instruction>p;p.reserve(lines.size());
 for(const auto&ln:lines){std::stringstream ss(ln.text);std::string op,args;ss>>op;std::getline(ss,args);op=upper(op);auto v=split(trim(args));
 auto target=[&](std::string s){s=trim(s);auto it=labels.find(upper(s));if(it!=labels.end())return it->second;auto ci=constants.find(upper(s));if(ci!=constants.end())return ci->second;return number(s,ln.n,65535);};
 for(auto& operand:v){auto ci=constants.find(upper(operand));if(ci!=constants.end())operand=std::to_string(ci->second);if(operand.size()>2&&operand.front()=='['&&operand.back()==']'){auto inner=trim(operand.substr(1,operand.size()-2));auto ii=constants.find(upper(inner));if(ii!=constants.end())operand="["+std::to_string(ii->second)+"]";}}
 try{
 if(op=="HALT"){if(!trim(args).empty())throw std::runtime_error("HALT takes no operands");p.push_back({Opcode::Halt,0});}
 else if(op=="PRINT"){if(v.size()!=1||upper(v[0])!="A")throw std::runtime_error("syntax: PRINT A");p.push_back({Opcode::Print,0});}
 else if(op=="ADD"||op=="SUB"){if(v.size()!=2||upper(v[0])!="A"||upper(v[1])!="B")throw std::runtime_error("syntax: "+op+" A, B");p.push_back({op=="ADD"?Opcode::Add:Opcode::Sub,0});}
 else if(op=="INC"||op=="DEC"){if(v.size()!=1||upper(v[0])!="A")throw std::runtime_error("syntax: "+op+" A");p.push_back({op=="INC"?Opcode::Inc:Opcode::Dec,0});}
 else if(op=="LOAD"){if(v.size()!=2||v[0].empty()||v[1].empty())throw std::runtime_error("syntax: LOAD A/B, value or LOAD A, [address]");auto reg=upper(v[0]);if(reg=="A"&&v[1].front()=='[')p.push_back({Opcode::Load,number(unbracket(v[1]),ln.n,65535)});else if(reg=="A")p.push_back({Opcode::LoadImmediate,number(v[1],ln.n,255)});else if(reg=="B")p.push_back({Opcode::LoadBImmediate,number(v[1],ln.n,255)});else throw std::runtime_error("register must be A or B");}
 else if(op=="SYS"){if(!trim(args).empty())throw std::runtime_error("SYS takes no operands; service number in A");p.push_back({Opcode::Syscall,0});}
 else if(op=="IN"||op=="OUT"){if(v.size()!=2||upper(v[0])!="A")throw std::runtime_error("syntax: "+op+" A, port");auto port=number(v[1],ln.n,255);if(op=="IN"&&port>3)throw std::runtime_error("IN ports: 0 keyboard, 2 graphics X, 3 graphics Y");if(op=="OUT"&&port>6)throw std::runtime_error("OUT ports: 0..6; display=1, graphics=2..6");p.push_back({op=="IN"?Opcode::Input:Opcode::Output,port});}
 else if(op=="CMP"){if(v.size()==2&&upper(v[0])=="A"&&upper(v[1])=="B")p.push_back({Opcode::Compare,0});else if(v.size()==2&&upper(v[0])=="A")p.push_back({Opcode::CompareImmediate,number(v[1],ln.n,255)});else throw std::runtime_error("syntax: CMP A, B or CMP A, value");}
 else if(op=="STORE"){if(v.size()!=1)throw std::runtime_error("syntax: STORE [address]");p.push_back({Opcode::Store,number(unbracket(v[0]),ln.n,65535)});}
 else if(op=="JMP"||op=="JZ"||op=="JNZ"||op=="CALL"){if(v.size()!=1)throw std::runtime_error("syntax: "+op+" label");auto t=target(v[0]);p.push_back({op=="JMP"?Opcode::Jump:op=="JZ"?Opcode::JumpZero:op=="JNZ"?Opcode::JumpNotZero:Opcode::Call,t});}
 else if(op=="PUSH"||op=="POP"){if(v.size()!=1)throw std::runtime_error("syntax: "+op+" A/B");auto r=upper(v[0]);if(r=="A")p.push_back({op=="PUSH"?Opcode::PushA:Opcode::PopA,0});else if(r=="B")p.push_back({op=="PUSH"?Opcode::PushB:Opcode::PopB,0});else throw std::runtime_error("register must be A or B");}
 else if(op=="RET"){if(!trim(args).empty())throw std::runtime_error("RET takes no operands");p.push_back({Opcode::Return,0});}
 else throw std::runtime_error("unknown instruction '"+op+"'");
 }catch(const std::exception&e){throw std::runtime_error("Assembly line "+std::to_string(ln.n)+": "+e.what());}}
 if(p.empty())throw std::runtime_error("Assembly program is empty");return p;
}
}