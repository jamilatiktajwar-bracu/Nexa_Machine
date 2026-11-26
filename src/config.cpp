#include "nexa/config.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace nexa {
namespace {
std::string valueFor(const std::string& text,const std::string& key) {
 const auto k=text.find("\\""+key+"\\""); if(k==std::string::npos)return {};
 const auto colon=text.find(':',k); if(colon==std::string::npos)return {};
 auto start=text.find_first_not_of(" \\t\\r\\n",colon+1); if(start==std::string::npos)return {};
 if(text[start]=='\\"'){const auto end=text.find('\\"',start+1);if(end==std::string::npos)return {};return text.substr(start+1,end-start-1);}
 auto end=text.find_first_of(",} \\t\\r\\n",start);return text.substr(start,end==std::string::npos?text.size()-start:end-start);
}
}
MachineConfig MachineConfig::load(const std::string& path) {
 std::ifstream in(path);if(!in)throw std::runtime_error("Cannot open config: "+path);
 std::string text((std::istreambuf_iterator<char>(in)),{});MachineConfig c;
 auto name=valueFor(text,"name");if(!name.empty())c.name=name;
 auto disk=valueFor(text,"disk_path");if(!disk.empty())c.diskPath=disk;
 auto ram=valueFor(text,"ram_bytes");if(!ram.empty())c.ramBytes=std::stoull(ram);
 auto size=valueFor(text,"disk_bytes");if(!size.empty())c.diskBytes=std::stoull(size);
 if(c.ramBytes==0||c.ramBytes>65536||c.diskBytes==0)throw std::runtime_error("Invalid config sizes");
 return c;
}
void MachineConfig::save(const std::string& path)const {
 std::ofstream out(path);if(!out)throw std::runtime_error("Cannot write config: "+path);
 out<<"{\\"name\\":\\""<<name<<"\\",\\"ram_bytes\\":"<<ramBytes<<",\\"disk_path\\":\\""<<diskPath<<"\\",\\"disk_bytes\\":"<<diskBytes<<"}";
}
}
