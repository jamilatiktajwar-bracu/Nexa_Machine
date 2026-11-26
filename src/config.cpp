#include "nexa/config.hpp"
#include <fstream>
#include <regex>
#include <stdexcept>
namespace nexa {
MachineConfig MachineConfig::load(const std::string& path) {
 std::ifstream f(path); if(!f) throw std::runtime_error("Cannot open config: "+path);
 std::string s((std::istreambuf_iterator<char>(f)),{}); MachineConfig c;
 auto str=[&](const char* key,std::string& v){std::regex r("\""+std::string(key)+"\"\\s*:\s*\"([^\"]*)\"");std::smatch m;if(std::regex_search(s,m,r))v=m[1];};
 auto num=[&](const char* key,std::size_t& v){std::regex r("\""+std::string(key)+"\"\\s*:\s*(\\d+)");std::smatch m;if(std::regex_search(s,m,r))v=std::stoull(m[1]);};
 str("name",c.name);str("disk_path",c.diskPath);num("ram_bytes",c.ramBytes);num("disk_bytes",c.diskBytes);
 if(!c.ramBytes||c.ramBytes>65536||!c.diskBytes)throw std::runtime_error("Invalid config size");
 return c;
}
void MachineConfig::save(const std::string& path)const {
 std::ofstream f(path);if(!f)throw std::runtime_error("Cannot write config");
 f<<"{\n  \"name\": \""<<name<<"\",\n  \"ram_bytes\": "<<ramBytes<<",\n  \"disk_path\": \""<<diskPath<<"\",\n  \"disk_bytes\": "<<diskBytes<<"\n}\n";
}
}
