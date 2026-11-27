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
 std::filesystem::remove_all(paths.root);
 std::cout<<"Workspace path tests passed\n";
}
