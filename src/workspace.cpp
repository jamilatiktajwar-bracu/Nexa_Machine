#include "nexa/workspace.hpp"
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <stdexcept>
namespace nexa {
namespace {
std::string jsonEscape(const std::string& value) {
 std::string out;
 for(unsigned char c:value) {
  switch(c) { case '"': out+="\\\""; break; case '\\': out+="\\\\"; break;
   case '\n': out+="\\n"; break; case '\r': out+="\\r"; break; case '\t': out+="\\t"; break;
   default: if(c<0x20) throw std::invalid_argument("Profile path contains control characters"); out+=static_cast<char>(c);
  }
 }
 return out;
}
bool validProfileName(const std::string& name) {
 return !name.empty() && name.size()<=48 && std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isalnum(c)||c=='-'||c=='_';});
}
std::filesystem::path homePath() {
#ifdef _WIN32
 if (const char* p=std::getenv("USERPROFILE")) return p;
 if (const char* d=std::getenv("HOMEDRIVE")) { if(const char* h=std::getenv("HOMEPATH")) return std::filesystem::path(std::string(d)+h); }
#else
 if (const char* p=std::getenv("HOME")) return p;
#endif
 return std::filesystem::current_path();
}
}
WorkspacePaths WorkspacePaths::discover() {
 std::filesystem::path root;
 if(const char* overridePath=std::getenv("NEXA_HOME"); overridePath && *overridePath) root=overridePath;
 else root=homePath()/".nexa";
 return {root,root/"config",root/"profiles",root/"disks",root/"snapshots",root/"logs"};
}
void WorkspacePaths::initialize() const {
 std::filesystem::create_directories(config);
 std::filesystem::create_directories(profiles);
 std::filesystem::create_directories(disks);
 std::filesystem::create_directories(snapshots);
 std::filesystem::create_directories(logs);
}
std::filesystem::path WorkspacePaths::createProfile(const std::string& name) const {
 if(!validProfileName(name))
  throw std::invalid_argument("Profile name must be 1-48 characters: letters, digits, dash or underscore.");
 initialize();
 const auto target=profiles/(name+".json");
 if(std::filesystem::exists(target)) throw std::runtime_error("Profile already exists: "+name);
 std::ofstream out(target,std::ios::binary|std::ios::out);
 if(!out) throw std::runtime_error("Cannot create profile: "+target.string());
 out << "{\n  \"name\": \""<<jsonEscape(name)<<"\",\n  \"ram_bytes\": 256,\n  \"disk_path\": \""<<jsonEscape((disks/(name+".disk")).generic_string())<<"\",\n  \"disk_bytes\": 1048576\n}\n";
 out.close();
 if(!out) throw std::runtime_error("Failed writing profile file.");
 return target;
}
std::vector<std::string> WorkspacePaths::listProfiles() const {
 std::vector<std::string> result;
 if(!std::filesystem::exists(profiles)) return result;
 for(const auto& entry:std::filesystem::directory_iterator(profiles))
  if(entry.is_regular_file()&&entry.path().extension()==".json") result.push_back(entry.path().stem().string());
 std::sort(result.begin(),result.end());
 return result;
}
std::string WorkspacePaths::summary() const {
 std::ostringstream out;
 out<<"Workspace: "<<root.string()<<"\nConfig: "<<config.string()<<"\nProfiles: "<<profiles.string()
 <<"\nDisks: "<<disks.string()<<"\nSnapshots: "<<snapshots.string()<<"\nLogs: "<<logs.string();
 return out.str();
}
}
