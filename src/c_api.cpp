#include "nexa/c_api.h"
#include "nexa/session_manager.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
struct NexaContext {nexa::SessionManager sessions;std::map<uint64_t,std::string> ids;uint64_t next{1};std::string error;};
namespace {
template<class F>int guard(NexaContext*c,F&&fn){if(!c)return -1;try{fn();c->error.clear();return 0;}catch(const std::exception&e){c->error=e.what();return -1;}catch(...){c->error="Unknown Nexa error";return -1;}}
std::string session(NexaContext*c,uint64_t id){auto it=c->ids.find(id);if(it==c->ids.end())throw std::out_of_range("Unknown C API session handle");return it->second;}
NexaSessionState state(nexa::SessionState s){switch(s){case nexa::SessionState::Ready:return NEXA_READY;case nexa::SessionState::Running:return NEXA_RUNNING;case nexa::SessionState::Paused:return NEXA_PAUSED;case nexa::SessionState::Stopped:return NEXA_STOPPED;case nexa::SessionState::Halted:return NEXA_HALTED;case nexa::SessionState::Faulted:return NEXA_FAULTED;}return NEXA_FAULTED;}
}
extern "C" {
NexaContext*nexa_context_create(void){try{return new NexaContext;}catch(...){return nullptr;}}
void nexa_context_destroy(NexaContext*c){delete c;}
const char*nexa_last_error(NexaContext*c){return c?c->error.c_str():"Invalid context";}
int nexa_session_create(NexaContext*c,const char*name,size_t ram,uint64_t*out){return guard(c,[&]{if(!out)throw std::invalid_argument("Output handle pointer is null");auto id=c->sessions.create(name?name:"Nexa session",ram);auto handle=c->next++;c->ids.emplace(handle,std::move(id));*out=handle;});}
int nexa_session_remove(NexaContext*c,uint64_t id){return guard(c,[&]{auto key=session(c,id);if(!c->sessions.remove(key))throw std::runtime_error("Session removal failed");c->ids.erase(id);});}
int nexa_session_load_assembly(NexaContext*c,uint64_t id,const char*src){return guard(c,[&]{if(!src)throw std::invalid_argument("Assembly source is null");c->sessions.loadAssembly(session(c,id),src);});}
int nexa_session_start(NexaContext*c,uint64_t id){return guard(c,[&]{c->sessions.start(session(c,id));});}
int nexa_session_pause(NexaContext*c,uint64_t id){return guard(c,[&]{c->sessions.pause(session(c,id));});}
int nexa_session_resume(NexaContext*c,uint64_t id){return guard(c,[&]{c->sessions.resume(session(c,id));});}
int nexa_session_stop(NexaContext*c,uint64_t id){return guard(c,[&]{c->sessions.stop(session(c,id));});}
int nexa_session_reset(NexaContext*c,uint64_t id){return guard(c,[&]{c->sessions.reset(session(c,id));});}
int nexa_session_step(NexaContext*c,uint64_t id,int*alive){return guard(c,[&]{if(!alive)throw std::invalid_argument("Alive output pointer is null");*alive=c->sessions.step(session(c,id))?1:0;});}
int nexa_session_run(NexaContext*c,uint64_t id,size_t budget,NexaSessionState*out){return guard(c,[&]{auto result=c->sessions.run(session(c,id),budget);if(out)*out=state(result);});}
int nexa_session_info(NexaContext*c,uint64_t id,NexaSessionInfo*out){return guard(c,[&]{if(!out)throw std::invalid_argument("Info output pointer is null");auto s=c->sessions.info(session(c,id));std::memset(out,0,sizeof(*out));out->id=id;std::strncpy(out->name,s.name.c_str(),sizeof(out->name)-1);out->state=state(s.state);out->pc=s.pc;out->instructions=s.instructions;out->ram_bytes=s.ramBytes;out->halted=s.halted?1:0;});}
int nexa_memory_read(NexaContext*c,uint64_t id,uint16_t a,uint8_t*out){return guard(c,[&]{if(!out)throw std::invalid_argument("Memory output pointer is null");*out=c->sessions.machine(session(c,id)).readMemory(a);});}
int nexa_memory_write(NexaContext*c,uint64_t id,uint16_t a,uint8_t v){return guard(c,[&]{c->sessions.machine(session(c,id)).writeMemory(a,v);});}
int nexa_snapshot_save(NexaContext*c,uint64_t id,const char*path){return guard(c,[&]{if(!path)throw std::invalid_argument("Snapshot path is null");c->sessions.saveSnapshot(session(c,id),path);});}
int nexa_snapshot_load(NexaContext*c,uint64_t id,const char*path){return guard(c,[&]{if(!path)throw std::invalid_argument("Snapshot path is null");c->sessions.loadSnapshot(session(c,id),path);});}
}
