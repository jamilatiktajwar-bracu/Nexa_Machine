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
int nexa_interrupt_vector_set(NexaContext*,uint64_t id,uint8_t vector,uint16_t handler_address);
int nexa_framebuffer_get(NexaContext*,uint64_t id,size_t x,size_t y,int* out_pixel);
int nexa_framebuffer_draw(NexaContext*,uint64_t id,size_t x,size_t y,int white);
int nexa_snapshot_save(NexaContext*,uint64_t id,const char* path);
int nexa_snapshot_load(NexaContext*,uint64_t id,const char* path);
/* Coordinated bundle: VM state plus every attached disk image. Serialize context access. */
int nexa_snapshot_save_bundle(NexaContext*,uint64_t id,const char* vm_state_path,const char* disk_snapshot_directory);
int nexa_snapshot_restore_bundle(NexaContext*,uint64_t id,const char* vm_state_path,const char* disk_snapshot_directory);
/* 512-byte sectors. Guest SYS ABI: A=4 reads B=sector into RAM[0..511];
 * A=5 writes RAM[0..511] to B=sector. Requires >=512 bytes RAM. */
int nexa_disk_attach(NexaContext*,uint64_t id,const char* path,size_t capacity_bytes); /* device ID 0 compatibility */
int nexa_disk_attach_device(NexaContext*,uint64_t id,uint32_t device_id,const char* path,size_t capacity_bytes);
int nexa_disk_detach(NexaContext*,uint64_t id,uint32_t device_id,int flush_before_detach);
int nexa_disk_device_count(NexaContext*,uint64_t id,size_t* out_count);
int nexa_disk_sector_count_device(NexaContext*,uint64_t id,uint32_t device_id,size_t* out_count);
int nexa_disk_read_sector_device(NexaContext*,uint64_t id,uint32_t device_id,size_t sector,uint8_t* out_512_bytes);
int nexa_disk_write_sector_device(NexaContext*,uint64_t id,uint32_t device_id,size_t sector,const uint8_t* bytes_512);
int nexa_disk_flush_device(NexaContext*,uint64_t id,uint32_t device_id);
int nexa_disk_read_to_guest(NexaContext*,uint64_t id,uint32_t device_id,size_t sector,uint16_t guest_ram_address);
int nexa_disk_write_from_guest(NexaContext*,uint64_t id,uint32_t device_id,size_t sector,uint16_t guest_ram_address);
int nexa_disk_sector_count(NexaContext*,uint64_t id,size_t* out_count);
int nexa_disk_read_sector(NexaContext*,uint64_t id,size_t sector,uint8_t* out_512_bytes);
int nexa_disk_write_sector(NexaContext*,uint64_t id,size_t sector,const uint8_t* bytes_512);
int nexa_disk_flush(NexaContext*,uint64_t id);
/* 11-byte descriptor: op[0] (0 read/1 write), device[1..4],
 * sector[5..8], guest RAM buffer address[9..10], all little-endian. */
int nexa_disk_submit_guest(NexaContext*,uint64_t id,uint16_t descriptor_address,uint8_t completion_interrupt_vector,uint64_t* out_request_id);
/* Cooperative scheduler: executes at most one queued request per call. */
int nexa_disk_process_one(NexaContext*,uint64_t* out_completed_request_id);
/* status: 0=pending, 1=completed, -1=failed. */
int nexa_disk_request_status(NexaContext*,uint64_t request_id,int* out_status);
int nexa_disk_request_error(NexaContext*,uint64_t request_id,char* out_error,size_t capacity);
#ifdef __cplusplus
}
#endif
