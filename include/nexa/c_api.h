#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct NexaContext NexaContext;
typedef enum NexaSessionState { NEXA_READY=0,NEXA_RUNNING=1,NEXA_PAUSED=2,NEXA_STOPPED=3,NEXA_HALTED=4,NEXA_FAULTED=5 } NexaSessionState;
typedef struct NexaSessionInfo { uint64_t id; char name[128]; NexaSessionState state; size_t pc,instructions,ram_bytes; int halted; } NexaSessionInfo;
NexaContext* nexa_context_create(void);
void nexa_context_destroy(NexaContext*);
const char* nexa_last_error(NexaContext*);
int nexa_session_create(NexaContext*,const char* name,size_t ram_bytes,uint64_t* out_id);
int nexa_session_remove(NexaContext*,uint64_t id);
int nexa_session_load_assembly(NexaContext*,uint64_t id,const char* source);
int nexa_session_start(NexaContext*,uint64_t id);
int nexa_session_pause(NexaContext*,uint64_t id);
int nexa_session_resume(NexaContext*,uint64_t id);
int nexa_session_stop(NexaContext*,uint64_t id);
int nexa_session_reset(NexaContext*,uint64_t id);
int nexa_session_step(NexaContext*,uint64_t id,int* guest_alive);
int nexa_session_run(NexaContext*,uint64_t id,size_t budget,NexaSessionState* out_state);
int nexa_session_info(NexaContext*,uint64_t id,NexaSessionInfo* out_info);
/* Query with out_items=null, capacity=0; out_count receives required item count. */
int nexa_session_list(NexaContext*,NexaSessionInfo* out_items,size_t capacity,size_t* out_count);
int nexa_memory_read(NexaContext*,uint64_t id,uint16_t address,uint8_t* out_value);
int nexa_memory_write(NexaContext*,uint64_t id,uint16_t address,uint8_t value);
int nexa_keyboard_input(NexaContext*,uint64_t id,const char* text);
int nexa_breakpoint_set(NexaContext*,uint64_t id,size_t address);
int nexa_breakpoint_clear(NexaContext*,uint64_t id,size_t address);
int nexa_framebuffer_get(NexaContext*,uint64_t id,size_t x,size_t y,int* out_pixel);
int nexa_framebuffer_draw(NexaContext*,uint64_t id,size_t x,size_t y,int white);
int nexa_snapshot_save(NexaContext*,uint64_t id,const char* path);
int nexa_snapshot_load(NexaContext*,uint64_t id,const char* path);\n/* Host-side 512-byte sector image API; not yet guest-issued block I/O. */\nint nexa_disk_attach(NexaContext*,uint64_t id,const char* path,size_t capacity_bytes);\nint nexa_disk_sector_count(NexaContext*,uint64_t id,size_t* out_count);\nint nexa_disk_read_sector(NexaContext*,uint64_t id,size_t sector,uint8_t* out_512_bytes);\nint nexa_disk_write_sector(NexaContext*,uint64_t id,size_t sector,const uint8_t* bytes_512);\nint nexa_disk_flush(NexaContext*,uint64_t id);
#ifdef __cplusplus
}
#endif
