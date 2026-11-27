#include "nexa/workspace.hpp"
#include <cstdlib>
#include <sstream>
namespace nexa {
namespace {
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
std::string WorkspacePaths::summary() const {
 std::ostringstream out;
 out<<"Workspace: "<<root.string()<<"\nConfig: "<<config.string()<<"\nProfiles: "<<profiles.string()
 <<"\nDisks: "<<disks.string()<<"\nSnapshots: "<<snapshots.string()<<"\nLogs: "<<logs.string();
 return out.str();
}
}
