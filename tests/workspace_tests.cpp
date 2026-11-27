#include "nexa/workspace.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
int main(){
#ifdef _WIN32
 _putenv_s("NEXA_HOME","nexa-workspace-test");
#else
 setenv("NEXA_HOME","nexa-workspace-test",1);
#endif
 auto paths=nexa::WorkspacePaths::discover();
 assert(paths.root.filename()=="nexa-workspace-test");
 paths.initialize();
 assert(std::filesystem::is_directory(paths.config));
 assert(std::filesystem::is_directory(paths.profiles));
 assert(std::filesystem::is_directory(paths.disks));
 assert(std::filesystem::is_directory(paths.snapshots));
 assert(std::filesystem::is_directory(paths.logs));
 auto profile=paths.createProfile("portable_dev"); assert(std::filesystem::exists(profile));
 auto names=paths.listProfiles(); assert(names.size()==1 && names[0]=="portable_dev");
 bool rejected=false; try{paths.createProfile("../bad");}catch(const std::invalid_argument&){rejected=true;} assert(rejected);
 bool duplicate=false; try{paths.createProfile("portable_dev");}catch(const std::runtime_error&){duplicate=true;} assert(duplicate);
 std::filesystem::remove_all(paths.root);
 std::cout<<"Workspace path tests passed\n";
}
