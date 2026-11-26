#include "nexa/executable.hpp"
#include <fstream>
#include <stdexcept>
namespace nexa { namespace {
void put16(std::ostream&o,std::uint16_t v){o.put(static_cast<char>(v&255));o.put(static_cast<char>(v>>8));}
void put32(std::ostream&o,std::uint32_t v){for(int n=0;n<4;++n)o.put(static_cast<char>((v>>(8*n))&255));}
std::uint16_t get16(std::istream&i){int a=i.get(),b=i.get();if(a<0||b<0)throw std::runtime_error("Truncated executable");return static_cast<std::uint16_t>(a|(b<<8));}
std::uint32_t get32(std::istream&i){std::uint32_t v=0;for(int n=0;n<4;++n){int b=i.get();if(b<0)throw std::runtime_error("Truncated executable");v|=std::uint32_t(b)<<(8*n);}return v;}
void validate(const Executable&e){if(e.code.empty()||e.code.size()>65535||e.entryPoint>=e.code.size())throw std::invalid_argument("Invalid executable entry or code size");for(const auto& ins:e.code)if(static_cast<unsigned>(ins.opcode)>static_cast<unsigned>(Opcode::Halt))throw std::invalid_argument("Invalid executable opcode");}
}}
namespace nexa {
void saveExecutable(const Executable&e,const std::string&p){
 validate(e);
 std::ofstream o(p,std::ios::binary);if(!o)throw std::runtime_error("Cannot create executable: "+p);
 // ELF-like v2 header: machine, version, entry, instruction count, section table.
 o.write("NEXAELF2",8);put16(o,1);put16(o,2);put16(o,e.entryPoint);
 put32(o,static_cast<std::uint32_t>(e.code.size()));put16(o,1);
 put16(o,1); // section type: executable code
 put16(o,5); // flags: readable + executable, never writable
 put16(o,0); // virtual address
 put32(o,static_cast<std::uint32_t>(e.code.size()*3));
 for(const auto& ins:e.code){o.put(static_cast<char>(ins.opcode));put16(o,ins.operand);}
 if(!o)throw std::runtime_error("Executable write failed");
}
Executable loadExecutable(const std::string&p){
 std::ifstream i(p,std::ios::binary);if(!i)throw std::runtime_error("Cannot open executable: "+p);
 char magic[8];i.read(magic,8);if(!i)throw std::runtime_error("Truncated executable");
 const std::string signature(magic,8);Executable e;
 if(signature=="NEXAEXE1"){
   e.entryPoint=get16(i);const auto n=get32(i);
   if(n==0||n>65535||e.entryPoint>=n)throw std::runtime_error("Invalid legacy executable metadata");
   e.code.reserve(n);
   for(std::uint32_t k=0;k<n;++k){const int op=i.get();if(op<0||op>static_cast<int>(Opcode::Halt))throw std::runtime_error("Invalid executable opcode");e.code.push_back({static_cast<Opcode>(op),get16(i)});}
 } else if(signature=="NEXAELF2"){
   const auto machine=get16(i),version=get16(i);e.entryPoint=get16(i);const auto n=get32(i);const auto sections=get16(i);
   if(machine!=1||version!=2||n==0||n>65535||e.entryPoint>=n||sections!=1)throw std::runtime_error("Unsupported executable machine, version, or section table");
   const auto type=get16(i),flags=get16(i),address=get16(i),size=get32(i);
   if(type!=1||flags!=5||address!=0||size!=n*3u)throw std::runtime_error("Invalid executable code section");
   e.code.reserve(n);
   for(std::uint32_t k=0;k<n;++k){const int op=i.get();if(op<0||op>static_cast<int>(Opcode::Halt))throw std::runtime_error("Invalid executable opcode");const auto operand=get16(i);e.code.push_back({static_cast<Opcode>(op),operand});}
 } else throw std::runtime_error("Bad Nexa executable signature");
 if(i.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Executable has trailing bytes");
 return e;
}
}
