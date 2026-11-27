#include "nexa/session_manager.hpp"
#include "nexa/assembler.hpp"
#include <stdexcept>
#include <utility>
namespace nexa {
SessionManager::Session& SessionManager::require(const std::string&id){auto it=sessions_.find(id);if(it==sessions_.end())throw std::out_of_range("Unknown VM session: "+id);return it->second;}
const SessionManager::Session& SessionManager::require(const std::string&id)const{auto it=sessions_.find(id);if(it==sessions_.end())throw std::out_of_range("Unknown VM session: "+id);return it->second;}
std::string SessionManager::create(std::string name,std::size_t ram){if(name.empty())name="Nexa session";const auto id="session-"+std::to_string(nextId_++);Session s;s.id=id;s.name=std::move(name);s.vm=std::make_unique<VM>(ram);sessions_.emplace(id,std::move(s));return id;}
bool SessionManager::remove(const std::string&id){return sessions_.erase(id)!=0;}
std::vector<SessionInfo> SessionManager::list()const{std::vector<SessionInfo>out;out.reserve(sessions_.size());for(const auto&[id,s]:sessions_)out.push_back({id,s.name,s.state,s.vm->programCounter(),s.vm->instructionsExecuted(),s.vm->ramSize(),s.vm->halted()});return out;}
SessionInfo SessionManager::info(const std::string&id)const{const auto&s=require(id);return{id,s.name,s.state,s.vm->programCounter(),s.vm->instructionsExecuted(),s.vm->ramSize(),s.vm->halted()};}
void SessionManager::loadAssembly(const std::string&id,const std::string&src){auto&s=require(id);if(s.state==SessionState::Running)throw std::logic_error("Pause or stop session before loading code");s.vm->loadProgram(parseAssembly(src));s.programLoaded=true;s.state=SessionState::Ready;}
void SessionManager::start(const std::string&id){auto&s=require(id);if(!s.programLoaded)throw std::logic_error("No guest program loaded");if(s.state!=SessionState::Ready)throw std::logic_error("Start is only valid for a ready session");if(s.vm->halted())throw std::logic_error("Reset halted session before starting");s.state=SessionState::Running;}
void SessionManager::pause(const std::string&id){auto&s=require(id);if(s.state!=SessionState::Running)throw std::logic_error("Session is not running");s.state=SessionState::Paused;}
void SessionManager::resume(const std::string&id){auto&s=require(id);if(s.state!=SessionState::Paused)throw std::logic_error("Session is not paused");if(s.vm->halted())throw std::logic_error("Cannot resume a halted VM");s.state=SessionState::Running;}
void SessionManager::stop(const std::string&id){auto&s=require(id);s.vm->requestHalt();s.state=SessionState::Stopped;}
void SessionManager::reset(const std::string&id){auto&s=require(id);s.vm->reset();s.state=SessionState::Ready;}
bool SessionManager::step(const std::string&id){auto&s=require(id);if(!s.programLoaded)throw std::logic_error("No guest program loaded");if(s.state==SessionState::Stopped||s.state==SessionState::Halted||s.state==SessionState::Faulted)throw std::logic_error("Reset session before stepping");try{const bool alive=s.vm->stepInstruction();s.state=alive?SessionState::Paused:SessionState::Halted;return alive;}catch(...){s.state=SessionState::Faulted;throw;}}
SessionState SessionManager::run(const std::string&id,std::size_t budget){auto&s=require(id);if(!s.programLoaded)throw std::logic_error("No guest program loaded");if(s.state!=SessionState::Running)throw std::logic_error("Start or resume session before running");std::size_t executed=0;try{while(s.state==SessionState::Running&&!s.vm->halted()&&executed<budget){if(s.vm->hasBreakpoint(s.vm->programCounter())){s.state=SessionState::Paused;break;}s.vm->stepInstruction();++executed;}if(s.vm->halted())s.state=SessionState::Halted;else if(s.state==SessionState::Running)s.state=SessionState::Paused;return s.state;}catch(...){s.state=SessionState::Faulted;throw;}}
void SessionManager::saveSnapshot(const std::string&id,const std::string&path)const{require(id).vm->saveState(path);}
void SessionManager::loadSnapshot(const std::string&id,const std::string&path){auto&s=require(id);if(s.state==SessionState::Running)throw std::logic_error("Pause session before restoring");s.vm->loadState(path);s.programLoaded=true;s.state=s.vm->halted()?SessionState::Halted:SessionState::Paused;}
VM& SessionManager::machine(const std::string&id){return *require(id).vm;}
const VM& SessionManager::machine(const std::string&id)const{return *require(id).vm;}
}
