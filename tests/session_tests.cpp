#include "nexa/session_manager.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
int main(){
 nexa::SessionManager manager;auto one=manager.create("alpha",256);auto two=manager.create("beta",512);
 assert(manager.list().size()==2);assert(manager.info(one).ramBytes==256);assert(manager.info(two).ramBytes==512);
 manager.loadAssembly(one,"LOAD A, 7\nINC A\nPRINT A\nHALT\n");manager.loadAssembly(two,"LOAD A, 40\nHALT\n");
 manager.start(one);assert(manager.info(one).state==nexa::SessionState::Running);assert(manager.run(one,1)==nexa::SessionState::Paused);assert(manager.machine(one).accumulator()==7);manager.resume(one);assert(manager.run(one,10)==nexa::SessionState::Halted);assert(manager.machine(one).accumulator()==8);
 assert(manager.info(two).instructions==0);manager.start(two);assert(manager.run(two,10)==nexa::SessionState::Halted);assert(manager.machine(two).accumulator()==40);
 manager.reset(one);assert(manager.info(one).state==nexa::SessionState::Ready);manager.start(one);manager.machine(one).setBreakpoint(1);assert(manager.run(one,10)==nexa::SessionState::Paused);assert(manager.info(one).pc==1);manager.machine(one).clearBreakpoints();assert(manager.run(one,10)==nexa::SessionState.Halted);
 manager.stop(two);assert(manager.info(two).state==nexa::SessionState::Stopped);manager.reset(two);assert(manager.info(two).state==nexa::SessionState::Ready);
 bool threw=false;try{manager.info("missing");}catch(const std::out_of_range&){threw=true;}assert(threw);assert(manager.remove(two));assert(manager.list().size()==1);
 auto state=(std::filesystem::temp_directory_path()/"nexa-session-test.state").string();manager.saveSnapshot(one,state);manager.reset(one);manager.loadSnapshot(one,state);assert(manager.info(one).pc==4);std::remove(state.c_str());
 std::cout<<"Session manager tests passed\n";
}
