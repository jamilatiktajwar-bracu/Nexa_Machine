#include "nexa/executable.hpp"
#include <fstream>
#include <stdexcept>
namespace nexa { namespace {
void put16(std::ostream&o,std::uint16_t v){o.put(char(v&255));o.put(char(v>>8));}
void put32(std::ostream&o,std::uint32_t v){for(int i=0;i<4;++i)o.put(char((v>>(8*i))&255));}
std::uint16_t get16(std::istream&i){int a=i.get(),b=i.get();if(a<0||b<0)throw std::runtime_error("Truncated executable");return std::uint16_t(a|(b<<8));}
std::uint32_t get32(std::istream&i){std::uint32_t v=0;for(int n=0;n<4;++n){int b=i.get();if(b<0)throw std::runtime_error("Truncated executable");v|=std::uint32_t(b)<<(8*n);}return v;}
}}
namespace nexa {
void saveExecutable(const Executable&e,const std::string&p){if(e.code.empty()||e.code.size()>65535||e.entryPoint>=e.code.size())throw std::invalid_argument("Invalid executable entry or code size");std::ofstream o(p,std::ios::binary);if(!o)throw std::runtime_error("Cannot create executable: "+p);o.write("NEXAEXE1",8);put16(o,e.entryPoint);put32(o,static_cast<std::uint32_t>(e.code.size()));for(auto ins:e.code){o.put(char(ins.opcode));put16(o,ins.operand);}if(!o)throw std::runtime_error("Executable write failed");}
Executable loadExecutable(const std::string&p){std::ifstream i(p,std::ios::binary);if(!i)throw std::runtime_error("Cannot open executable: "+p);char m[8];i.read(m,8);if(!i||std::string(m,8)!="NEXAEXE1")throw std::runtime_error("Bad Nexa executable signature");Executable e;e.entryPoint=get16(i);auto n=get32(i);if(n==0||n>65535||e.entryPoint>=n)throw std::runtime_error("Invalid executable metadata");e.code.reserve(n);for(std::uint32_t k=0;k<n;++k){int op=i.get();if(op<0||op>int(Opcode::Halt))throw std::runtime_error("Invalid executable opcode");e.code.push_back({static_cast<Opcode>(op),get16(i)});}if(i.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Executable has trailing bytes");return e;}
}
