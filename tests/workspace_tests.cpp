#include "nexa/workspace.hpp"
#include "nexa/config.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <stdexcept>
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
 auto cfg=nexa::MachineConfig::load(profile.string()); assert(cfg.name=="portable_dev" && cfg.ramBytes==256 && cfg.diskBytes==1048576);
 auto custom=paths.createProfile("custom_vm",4096,8388608); auto customCfg=nexa::MachineConfig::load(custom.string()); assert(customCfg.ramBytes==4096&&customCfg.diskBytes==8388608);
 bool invalidSize=false;try{paths.createProfile("badsize",0,100);}catch(const std::invalid_argument&){invalidSize=true;}assert(invalidSize); bool tooLarge=false;try{paths.createProfile("oversize",1024,134217729);}catch(const std::invalid_argument&){tooLarge=true;}assert(tooLarge); assert(cfg.diskPath.find("portable_dev.disk")!=std::string::npos);
 auto snap=paths.snapshotPath("portable_dev","before-run"); assert(snap.filename()=="portable_dev-before-run.state"); {std::ofstream temp(snap);temp<<"test";} auto snames=paths.listSnapshots("portable_dev"); assert(snames.size()==1&&snames[0]=="before-run"); paths.deleteSnapshot("portable_dev","before-run"); assert(paths.listSnapshots("portable_dev").empty());
 auto names=paths.listProfiles(); assert(names.size()==2 && names[0]=="custom_vm" && names[1]=="portable_dev");
 bool rejected=false; try{paths.createProfile("../bad");}catch(const std::invalid_argument&){rejected=true;} assert(rejected);
 bool duplicate=false; try{paths.createProfile("portable_dev");}catch(const std::runtime_error&){duplicate=true;} assert(duplicate);
 paths.deleteProfile("custom_vm"); assert(paths.listProfiles().size()==1); bool missing=false;try{paths.deleteProfile("missing");}catch(const std::runtime_error&){missing=true;}assert(missing);
 std::filesystem::remove_all(paths.root);
 std::cout<<"Workspace path tests passed\n";
}
