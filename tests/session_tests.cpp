#include "nexa/session_manager.hpp"
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>

static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 nexa::SessionManager manager;auto one=manager.create("alpha",256);auto two=manager.create("beta",512);
 check(manager.list().size()==2,"two sessions created");check(manager.info(one).ramBytes==256,"alpha RAM");check(manager.info(two).ramBytes==512,"beta RAM");
 manager.loadAssembly(one,"LOAD A, 7\\nINC A\\nPRINT A\\nHALT\\n");manager.loadAssembly(two,"LOAD A, 40\\nHALT\\n");
 manager.start(one);check(manager.info(one).state==nexa::SessionState::Running,"alpha running");auto state=manager.run(one,1);check(state==nexa::SessionState::Paused,"budget pauses alpha");check(manager.machine(one).accumulator()==7,"alpha accumulator after one step");manager.resume(one);state=manager.run(one,10);check(state==nexa::SessionState::Halted,"alpha halts");check(manager.machine(one).accumulator()==8,"alpha final accumulator");
 check(manager.info(two).instructions==0,"sessions isolated");manager.start(two);state=manager.run(two,10);check(state==nexa::SessionState::Halted,"beta halts");check(manager.machine(two).accumulator()==40,"beta accumulator");
 manager.reset(one);check(manager.info(one).state==nexa::SessionState::Ready,"reset ready");manager.start(one);manager.machine(one).setBreakpoint(1);state=manager.run(one,10);check(state==nexa::SessionState::Paused,"breakpoint pauses");check(manager.info(one).pc==1,"breakpoint PC");manager.machine(one).clearBreakpoints();manager.resume(one);state=manager.run(one,10);check(state==nexa::SessionState::Halted,"resumed halt");
 manager.stop(two);check(manager.info(two).state==nexa::SessionState::Stopped,"stop state");manager.reset(two);check(manager.info(two).state==nexa::SessionState::Ready,"stop reset");
 bool threw=false;try{manager.info("missing");}catch(const std::out_of_range&){threw=true;}check(threw,"unknown session throws");check(manager.remove(two),"remove beta");check(manager.list().size()==1,"one session remains");
 auto path=(std::filesystem::temp_directory_path()/"nexa-session-test.state").string();manager.saveSnapshot(one,path);manager.reset(one);manager.loadSnapshot(one,path);check(manager.info(one).pc==4,"snapshot restore PC");std::remove(path.c_str());
 std::cout<<"Session manager tests passed\\n";return 0;
}catch(const std::exception&e){std::cerr<<"Session manager test failure: "<<e.what()<<"\\n";return 1;}}
