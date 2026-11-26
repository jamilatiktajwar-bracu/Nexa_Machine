#include "nexa/assembler.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
namespace nexa {namespace {
std::string trim(std::string s){auto a=s.find_first_not_of(" \t\r\n");if(a==std::string::npos)return {};auto b=s.find_last_not_of(" \t\r\n");return s.substr(a,b-a+1);}
std::string upper(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});return s;}
std::vector<std::string>split(const std::string&s){std::stringstream ss(s);std::vector<std::string>v;std::string p;while(std::getline(ss,p,','))v.push_back(trim(p));return v;}
std::uint16_t number(std::string s,std::size_t line,std::uint32_t max){s=trim(s);try{std::size_t used=0;auto v=std::stoul(s,&used,0);if(used!=s.size()||v>max)throw std::runtime_error("range");return static_cast<std::uint16_t>(v);}catch(...){throw std::runtime_error("Assembly line "+std::to_string(line)+": invalid number '"+s+"'");}}
std::string unbracket(std::string s){s=trim(s);if(s.size()>=2&&s.front()=='['&&s.back()==']')return trim(s.substr(1,s.size()-2));return s;}
}
std::vector<Instruction>parseAssembly(const std::string&src){std::vector<Instruction>p;std::stringstream in(src);std::string line;std::size_t n=0;while(std::getline(in,line)){++n;auto c=line.find_first_of("#;");if(c!=std::string::npos)line.erase(c);line=trim(line);if(line.empty())continue;std::stringstream ss(line);std::string op,args;ss>>op;std::getline(ss,args);op=upper(op);auto v=split(trim(args));try{
if(op=="HALT"){if(!trim(args).empty())throw std::runtime_error("HALT takes no operands");p.push_back({Opcode::Halt,0});}
else if(op=="PRINT"){if(v.size()!=1||upper(v[0])!="A")throw std::runtime_error("syntax: PRINT A");p.push_back({Opcode::Print,0});}
else if(op=="ADD"){if(v.size()!=2||upper(v[0])!="A"||upper(v[1])!="B")throw std::runtime_error("syntax: ADD A, B");p.push_back({Opcode::Add,0});}
else if(op=="LOAD"){if(v.size()!=2||v[0].empty()||v[1].empty())throw std::runtime_error("syntax: LOAD A/B, value or LOAD A, [address]");auto reg=upper(v[0]);if(reg=="A"&&v[1].front()=='[')p.push_back({Opcode::Load,number(unbracket(v[1]),n,65535)});else if(reg=="A")p.push_back({Opcode::LoadImmediate,number(v[1],n,255)});else if(reg=="B")p.push_back({Opcode::LoadBImmediate,number(v[1],n,255)});else throw std::runtime_error("register must be A or B");}
else if(op=="STORE"){if(v.size()!=1)throw std::runtime_error("syntax: STORE [address]");p.push_back({Opcode::Store,number(unbracket(v[0]),n,65535)});}
else throw std::runtime_error("unknown instruction '"+op+"'");
}catch(const std::exception&e){throw std::runtime_error("Assembly line "+std::to_string(n)+": "+e.what());}}
if(p.empty())throw std::runtime_error("Assembly program is empty");return p;}
}
