#include "nexa/config.hpp"
#include <fstream>
#include <stdexcept>
namespace nexa {
namespace {
std::string valueFor(const std::string& text,const std::string& key) {
 const char q=char(34); const auto k=text.find(q+key+q); if(k==std::string::npos)return {};
 const auto colon=text.find(':',k);if(colon==std::string::npos)return {};
 auto start=text.find_first_not_of(" \t\r\n",colon+1);if(start==std::string::npos)return {};
 if(text[start]==q){auto end=text.find(q,start+1);if(end==std::string::npos)return {};return text.substr(start+1,end-start-1);}
 auto end=text.find_first_of(",} \t\r\n",start);return text.substr(start,end==std::string::npos?text.size()-start:end-start);
}
}
MachineConfig MachineConfig::load(const std::string& path) {
 std::ifstream in(path);if(!in)throw std::runtime_error("Cannot open config: "+path);
 std::string text((std::istreambuf_iterator<char>(in)),{});MachineConfig c;
 auto n=valueFor(text,"name");if(!n.empty())c.name=n;
 auto d=valueFor(text,"disk_path");if(!d.empty())c.diskPath=d;
 auto r=valueFor(text,"ram_bytes");if(!r.empty())c.ramBytes=std::stoull(r);
 auto s=valueFor(text,"disk_bytes");if(!s.empty())c.diskBytes=std::stoull(s);
 if(c.ramBytes==0||c.ramBytes>65536||c.diskBytes==0)throw std::runtime_error("Invalid config sizes");
 return c;
}
void MachineConfig::save(const std::string& path)const {
 std::ofstream out(path);if(!out)throw std::runtime_error("Cannot write config: "+path);
 const char q=char(34);
 out<<'{'<<q<<"name"<<q<<':'<<q<<name<<q<<','<<q<<"ram_bytes"<<q<<':'<<ramBytes<<','<<q<<"disk_path"<<q<<':'<<q<<diskPath<<q<<','<<q<<"disk_bytes"<<q<<':'<<diskBytes<<'}';
}
}
