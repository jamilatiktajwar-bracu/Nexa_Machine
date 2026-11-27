#include "nexa/c_api.h"
#include "nexa/session_manager.hpp"\n#include "nexa/virtual_disk.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
struct NexaContext {nexa::SessionManager sessions;std::map<uint64_t,std::string> ids;std::map<uint64_t,std::unique_ptr<nexa::VirtualDisk>> disks;uint64_t next{1};std::string error;};
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
int nexa_session_list(NexaContext*c,NexaSessionInfo*out,size_t capacity,size_t*outCount){return guard(c,[&]{if(!outCount)throw std::invalid_argument("Session count output pointer is null");auto items=c->sessions.list();*outCount=items.size();if(!out&&capacity!=0)throw std::invalid_argument("Session output array is null");if(!out&&capacity==0)return;if(capacity<items.size())throw std::length_error("Session output capacity is too small; query required count first");size_t i=0;for(const auto&s:items){auto&v=out[i++];std::memset(&v,0,sizeof(v));for(const auto&[handle,key]:c->ids){if(key==s.id){v.id=handle;break;}}std::strncpy(v.name,s.name.c_str(),sizeof(v.name)-1);v.state=state(s.state);v.pc=s.pc;v.instructions=s.instructions;v.ram_bytes=s.ramBytes;v.halted=s.halted?1:0;}});}
int nexa_session_info(NexaContext*c,uint64_t id,NexaSessionInfo*out){return guard(c,[&]{if(!out)throw std::invalid_argument("Info output pointer is null");auto s=c->sessions.info(session(c,id));std::memset(out,0,sizeof(*out));out->id=id;std::strncpy(out->name,s.name.c_str(),sizeof(out->name)-1);out->state=state(s.state);out->pc=s.pc;out->instructions=s.instructions;out->ram_bytes=s.ramBytes;out->halted=s.halted?1:0;});}
int nexa_memory_read(NexaContext*c,uint64_t id,uint16_t a,uint8_t*out){return guard(c,[&]{if(!out)throw std::invalid_argument("Memory output pointer is null");*out=c->sessions.machine(session(c,id)).readMemory(a);});}
int nexa_memory_write(NexaContext*c,uint64_t id,uint16_t a,uint8_t v){return guard(c,[&]{c->sessions.machine(session(c,id)).writeMemory(a,v);});}
int nexa_keyboard_input(NexaContext*c,uint64_t id,const char*text){return guard(c,[&]{if(!text)throw std::invalid_argument("Keyboard text is null");c->sessions.machine(session(c,id)).queueKeyboardInput(text);});}
int nexa_breakpoint_set(NexaContext*c,uint64_t id,size_t a){return guard(c,[&]{c->sessions.machine(session(c,id)).setBreakpoint(a);});}
int nexa_breakpoint_clear(NexaContext*c,uint64_t id,size_t a){return guard(c,[&]{c->sessions.machine(session(c,id)).clearBreakpoint(a);});}
int nexa_framebuffer_get(NexaContext*c,uint64_t id,size_t x,size_t y,int*out){return guard(c,[&]{if(!out)throw std::invalid_argument("Pixel output pointer is null");if(x>=nexa::VM::ScreenWidth||y>=nexa::VM::ScreenHeight)throw std::out_of_range("Pixel outside framebuffer");*out=c->sessions.machine(session(c,id)).pixel(x,y)?1:0;});}
int nexa_framebuffer_draw(NexaContext*c,uint64_t id,size_t x,size_t y,int white){return guard(c,[&]{if(x>=nexa::VM::ScreenWidth||y>=nexa::VM::ScreenHeight)throw std::out_of_range("Pixel outside framebuffer");c->sessions.machine(session(c,id)).drawPixel(x,y,white!=0);});}
int nexa_snapshot_save(NexaContext*c,uint64_t id,const char*path){return guard(c,[&]{if(!path)throw std::invalid_argument("Snapshot path is null");c->sessions.saveSnapshot(session(c,id),path);});}
int nexa_snapshot_load(NexaContext*c,uint64_t id,const char*path){return guard(c,[&]{if(!path)throw std::invalid_argument("Snapshot path is null");c->sessions.loadSnapshot(session(c,id),path);});}\nint nexa_disk_attach(NexaContext*c,uint64_t id,const char*path,size_t capacity){return guard(c,[&]{(void)session(c,id);if(!path||!*path)throw std::invalid_argument("Disk path is empty");if(capacity<512||capacity%512!=0)throw std::invalid_argument("Disk capacity must be a positive multiple of 512 bytes");auto disk=std::make_unique<nexa::VirtualDisk>(path,capacity);disk->load();c->disks[id]=std::move(disk);});}
int nexa_disk_sector_count(NexaContext*c,uint64_t id,size_t*out){return guard(c,[&]{if(!out)throw std::invalid_argument("Sector count output pointer is null");auto it=c->disks.find(id);if(it==c->disks.end())throw std::out_of_range("No disk attached to session");*out=it->second->size()/512;});}
int nexa_disk_read_sector(NexaContext*c,uint64_t id,size_t sector,uint8_t*out){return guard(c,[&]{if(!out)throw std::invalid_argument("Sector output buffer is null");auto it=c->disks.find(id);if(it==c->disks.end())throw std::out_of_range("No disk attached to session");auto data=it->second->read(sector*512,512);std::memcpy(out,data.data(),512);});}
int nexa_disk_write_sector(NexaContext*c,uint64_t id,size_t sector,const uint8_t*data){return guard(c,[&]{if(!data)throw std::invalid_argument("Sector input buffer is null");auto it=c->disks.find(id);if(it==c->disks.end())throw std::out_of_range("No disk attached to session");it->second->write(sector*512,std::vector<uint8_t>(data,data+512));});}
int nexa_disk_flush(NexaContext*c,uint64_t id){return guard(c,[&]{auto it=c->disks.find(id);if(it==c->disks.end())throw std::out_of_range("No disk attached to session");it->second->flush();});}

}
