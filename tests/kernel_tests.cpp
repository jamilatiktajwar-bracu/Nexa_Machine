#include "nexa/kernel.hpp"
#include "nexa/shell.hpp"
#define CHECK(expr) do { if (!(expr)) { std::cerr << "CHECK failed: " << #expr << " at line " << __LINE__ << '\n'; return 1; } } while (false
#include <iostream>
#define CHECK(expr) do { if (!(expr)) { std::cerr << "CHECK failed: " << #expr << " at line " << __LINE__ << '\\n'; return 1; } } while (false
#include <cstdio>
#include <filesystem>
#include <stdexcept>
int main() {
    nexa::NexaSecurity freshSecurity;
    nexa::NexaSecurity restoredFreshSecurity;
    restoredFreshSecurity.deserialize(freshSecurity.serialize());
    CHECK(restoredFreshSecurity.hasUser("root"));
    assert(!restoredFreshSecurity.authenticate("root", "not-configured"));


    nexa::NexaKernel schedulingKernel(512);
    const nexa::Executable looping{0,{{nexa::Opcode::Inc,0},{nexa::Opcode::Jump,0}}};
    const auto lowPid=schedulingKernel.spawn("low-priority",looping);
    const auto highPid=schedulingKernel.spawn("high-priority",looping);
    schedulingKernel.setPriority(lowPid,1);
    schedulingKernel.setPriority(highPid,7);
    schedulingKernel.setTimeQuantum(2);
    assert(schedulingKernel.scheduleOne());
    auto scheduled=schedulingKernel.processes();
    std::uint64_t lowTicks=0,highTicks=0;
    for(const auto& proc:scheduled){if(proc.pid==lowPid)lowTicks=proc.ticks;if(proc.pid==highPid)highTicks=proc.ticks;}
    assert(highTicks==2&&lowTicks==0);
    schedulingKernel.setTimerInterval(1);
    assert(schedulingKernel.scheduleOne());
    assert(schedulingKernel.systemTicks()==3&&schedulingKernel.timerInterrupts()==1);
    assert(schedulingKernel.pendingTimerInterrupts()==1);
    assert(schedulingKernel.acknowledgeTimerInterrupt());
    assert(!schedulingKernel.acknowledgeTimerInterrupt());
    const auto peerPid=schedulingKernel.spawn("equal-priority-peer",looping);
    schedulingKernel.setPriority(peerPid,7);
    assert(schedulingKernel.scheduleOne());
    scheduled=schedulingKernel.processes();
    bool peerRan=false;
    for(const auto& proc:scheduled)if(proc.pid==peerPid)peerRan=proc.ticks==1;
    assert(peerRan);
    bool invalidPriority=false;
    try{schedulingKernel.setPriority(peerPid,8);}catch(const std::invalid_argument&){invalidPriority=true;}
    assert(invalidPriority);

    nexa::NexaKernel budgetKernel(512);
    const auto budgetPid=budgetKernel.spawn("bounded-loop",looping);
    budgetKernel.setTimeQuantum(3);
    nexa::NexaShell budgetShell(budgetKernel);
    assert(budgetShell.execute("limit "+std::to_string(budgetPid)+" 5")=="instruction budget updated");
    assert(budgetShell.execute("ps").find("budget=5")!=std::string::npos);
    budgetKernel.run();
    auto budgetProcesses=budgetKernel.processes();
    bool budgetStopped=false;
    for(const auto& proc:budgetProcesses)
        if(proc.pid==budgetPid) budgetStopped=proc.state==nexa::ProcessState::Halted&&proc.ticks==5&&proc.exitCode==124&&proc.terminationReason=="budget";
    assert(budgetStopped);
    assert(budgetKernel.reapProcess(budgetPid));

    nexa::NexaKernel agingKernel(512);
    const auto agedLowPid=agingKernel.spawn("aged-low",looping);
    const auto agingHighPid=agingKernel.spawn("always-ready-high",looping);
    agingKernel.setPriority(agedLowPid,0);
    agingKernel.setPriority(agingHighPid,7);
    agingKernel.setPriorityAgingThreshold(1);
    agingKernel.setTimeQuantum(1);
    for(int tick=0;tick<7;++tick) assert(agingKernel.scheduleOne());
    auto agedProcesses=agingKernel.processes();
    bool agingReachedMaximum=false;
    for(const auto& proc:agedProcesses)if(proc.pid==agedLowPid)
        agingReachedMaximum=proc.effectivePriority==7&&proc.readyWaitTicks==7;
    assert(agingReachedMaximum);
    assert(agingKernel.scheduleOne());
    agedProcesses=agingKernel.processes();
    bool agedProcessRan=false;
    for(const auto& proc:agedProcesses)if(proc.pid==agedLowPid)agedProcessRan=proc.ticks==1;
    assert(agedProcessRan);

    nexa::NexaShell schedulerShell(schedulingKernel);
    assert(schedulerShell.execute("nice "+std::to_string(lowPid)+" 2")=="priority updated");
    assert(schedulerShell.execute("aging 1")=="priority aging updated");
    assert(schedulerShell.execute("quantum 4")=="time quantum updated");
    assert(schedulerShell.execute("timer 2")=="timer interval updated");
    assert(schedulerShell.execute("sched").find("policy=fixed-priority-round-robin")!=std::string::npos);
    assert(schedulerShell.execute("sched").find("quantum=4")!=std::string::npos);
    assert(schedulerShell.execute("sched").find("timer_interval=2")!=std::string::npos);
    assert(schedulerShell.execute("sched").find("aging_threshold=1")!=std::string::npos);
    assert(schedulerShell.execute("ps").find("priority=2")!=std::string::npos);
    assert(schedulerShell.execute("ps").find("effective_priority=")!=std::string::npos);
    assert(schedulerShell.execute("nice "+std::to_string(lowPid)+" 9")=="invalid PID or priority (0-7)");

    nexa::NexaKernel yieldKernel(256);
    const auto yieldingPid=yieldKernel.spawn("voluntary-yield",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(nexa::Syscall::Yield)},
        {nexa::Opcode::Syscall,0},{nexa::Opcode::LoadImmediate,'Y'},{nexa::Opcode::Halt,0}}});
    yieldKernel.setTimeQuantum(4);
    assert(yieldKernel.scheduleOne());
    auto yieldProcesses=yieldKernel.processes();
    bool yieldedAfterSyscall=false;
    for(const auto& proc:yieldProcesses)if(proc.pid==yieldingPid)
        yieldedAfterSyscall=proc.state==nexa::ProcessState::Ready&&proc.ticks==2;
    assert(yieldedAfterSyscall);
    assert(yieldKernel.scheduleOne());
    yieldProcesses=yieldKernel.processes();
    bool yieldedProcessFinished=false;
    for(const auto& proc:yieldProcesses)if(proc.pid==yieldingPid)
        yieldedProcessFinished=proc.state==nexa::ProcessState::Halted&&proc.ticks==4;
    assert(yieldedProcessFinished);


    nexa::NexaKernel shellMemoryKernel(256);
    nexa::NexaShell memoryShell(shellMemoryKernel);
    assert(memoryShell.execute("memstat").find("frames_total=")!=std::string::npos);
    const auto shellSwapPath=(std::filesystem::temp_directory_path()/"nexa_shell_swap.img").string();
    assert(memoryShell.execute("swapfile "+shellSwapPath)=="disk swap configured");
    assert(shellMemoryKernel.memoryManager().swapFilePath()==shellSwapPath);
    assert(memoryShell.execute("memstat").find("swap_backend=disk")!=std::string::npos);
    std::remove(shellSwapPath.c_str());
    nexa::NexaKernel processControlKernel(256);
    const auto framesBeforeManagedProcess=processControlKernel.memoryManager().freeFrames();
    const auto managedPid=processControlKernel.spawn("managed-loop",looping);
    nexa::NexaShell processShell(processControlKernel);
    assert(processShell.execute("kill "+std::to_string(managedPid))=="process terminated");
    auto managedProcesses=processControlKernel.processes();
    assert(managedProcesses.size()==1&&managedProcesses.front().state==nexa::ProcessState::Halted&&managedProcesses.front().exitCode==137&&managedProcesses.front().terminationReason=="killed");
    assert(processShell.execute("reap "+std::to_string(managedPid))=="process reaped");
    assert(processControlKernel.processes().empty());
    assert(processControlKernel.memoryManager().freeFrames()==framesBeforeManagedProcess);
    assert(processShell.execute("reap "+std::to_string(managedPid))=="unknown or active process");

    nexa::NexaKernel syncKernel(512);
    syncKernel.setTimeQuantum(3);
    const auto semId=syncKernel.createSemaphore(0,2);
    const auto waiterPid=syncKernel.spawn("semaphore-waiter",nexa::Executable{0,{
        {nexa::Opcode::LoadBImmediate,semId},
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(nexa::Syscall::SemaphoreWait)},{nexa::Opcode::Syscall,0},{nexa::Opcode::Store,100},{nexa::Opcode::Halt,0}}});
    assert(syncKernel.scheduleOne());
    auto syncInfo=syncKernel.processes();
    bool waiterBlocked=false;for(const auto& proc:syncInfo)if(proc.pid==waiterPid)waiterBlocked=proc.state==nexa::ProcessState::Blocked;
    assert(waiterBlocked&&!syncKernel.scheduleOne());
    assert(syncKernel.semaphoreSignal(semId));
    assert(syncKernel.scheduleOne());
    assert(syncKernel.readProcessMemory(waiterPid,100)==1);
    assert(syncKernel.semaphoreSignal(semId));
    assert(syncKernel.semaphoreSignal(semId));
    assert(!syncKernel.semaphoreSignal(semId));
    bool badSemaphore=false;try{(void)syncKernel.createSemaphore(2,1);}catch(const std::invalid_argument&){badSemaphore=true;}
    assert(badSemaphore);
    const auto mutexId=syncKernel.createMutex();
    const auto ownerPid=syncKernel.spawn("mutex-owner",looping);
    const auto lockWaiterPid=syncKernel.spawn("mutex-waiter",nexa::Executable{0,{
        {nexa::Opcode::LoadBImmediate,mutexId},
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(nexa::Syscall::MutexLock)},{nexa::Opcode::Syscall,0},{nexa::Opcode::Store,101},{nexa::Opcode::Jump,4}}});
    assert(syncKernel.mutexLock(ownerPid,mutexId));
    assert(syncKernel.sendSignal(ownerPid,nexa::ProcessSignal::Stop));
    assert(syncKernel.scheduleOne());
    syncInfo=syncKernel.processes();
    waiterBlocked=false;for(const auto& proc:syncInfo)if(proc.pid==lockWaiterPid)waiterBlocked=proc.state==nexa::ProcessState::Blocked;
    assert(waiterBlocked);
    syncKernel.setPriority(ownerPid,1);
    syncKernel.setPriority(lockWaiterPid,7);
    syncInfo=syncKernel.processes();
    bool ownerBoosted=false;for(const auto& proc:syncInfo)if(proc.pid==ownerPid)ownerBoosted=proc.effectivePriority==7;
    assert(ownerBoosted);
    assert(syncKernel.syncDiagnostics().find("waiters=[")!=std::string::npos);
    assert(syncKernel.mutexUnlock(ownerPid,mutexId));
    syncInfo=syncKernel.processes();
    bool boostRemoved=false;for(const auto& proc:syncInfo)if(proc.pid==ownerPid)boostRemoved=proc.effectivePriority==1;
    assert(boostRemoved);
    assert(syncKernel.scheduleOne());
    assert(syncKernel.readProcessMemory(lockWaiterPid,101)==1);
    assert(!syncKernel.mutexUnlock(ownerPid,mutexId));
    assert(syncKernel.mutexUnlock(lockWaiterPid,mutexId));
    // Isolated owner-death handoff test so unrelated ready processes cannot interfere.
    nexa::NexaKernel deathKernel;
    const auto deathMutex=deathKernel.createMutex();
    const auto deathOwner=deathKernel.spawn("mutex-owner-dies",looping);
    const auto deathWaiter=deathKernel.spawn("mutex-death-waiter",nexa::Executable{0,{
        {nexa::Opcode::LoadBImmediate,deathMutex},
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(nexa::Syscall::MutexLock)},{nexa::Opcode::Syscall,0},{nexa::Opcode::Store,102},{nexa::Opcode::Jump,4}}});
    assert(deathKernel.mutexLock(deathOwner,deathMutex));
    deathKernel.setPriority(deathOwner,1);
    deathKernel.setPriority(deathWaiter,7);
    assert(deathKernel.sendSignal(deathOwner,nexa::ProcessSignal::Stop));
    bool deathWaiterBlocked=false;
    for(int retry=0;retry<20&&!deathWaiterBlocked;++retry) {
        assert(deathKernel.scheduleOne());
        const auto deathState=deathKernel.processes();
        const auto deathWaiterState=std::find_if(deathState.begin(),deathState.end(),[&](const auto& proc){return proc.pid==deathWaiter;});
        deathWaiterBlocked=deathWaiterState!=deathState.end()&&deathWaiterState->state==nexa::ProcessState::Blocked;
    }
    assert(deathWaiterBlocked);
    assert(deathKernel.terminateProcess(deathOwner));
    for(int retry=0;retry<20&&deathKernel.readProcessMemory(deathWaiter,102)!=1;++retry) assert(deathKernel.scheduleOne());
    assert(deathKernel.readProcessMemory(deathWaiter,102)==1);
    assert(deathKernel.mutexUnlock(deathWaiter,deathMutex));
    // Exiting semaphore waiters are removed, and an unconsumed grant is handed on.
    nexa::NexaKernel waiterCleanupKernel;
    const auto cleanupSemaphore=waiterCleanupKernel.createSemaphore(0,2);
    const auto abandonedWaiter=waiterCleanupKernel.spawn("abandoned-sem-waiter",looping);
    const auto survivingWaiter=waiterCleanupKernel.spawn("surviving-sem-waiter",looping);
    assert(!waiterCleanupKernel.semaphoreWait(abandonedWaiter,cleanupSemaphore));
    assert(!waiterCleanupKernel.semaphoreWait(survivingWaiter,cleanupSemaphore));
    assert(waiterCleanupKernel.semaphoreSignal(cleanupSemaphore));
    assert(waiterCleanupKernel.terminateProcess(abandonedWaiter));
    assert(waiterCleanupKernel.semaphoreWait(survivingWaiter,cleanupSemaphore));
    assert(waiterCleanupKernel.syncDiagnostics().find("waiters=0")!=std::string::npos);

    // A process killed while waiting for a mutex must not remain in its queue.
    const auto cleanupMutex=waiterCleanupKernel.createMutex();
    const auto cleanupOwner=waiterCleanupKernel.spawn("cleanup-mutex-owner",looping);
    const auto cleanupBlocked=waiterCleanupKernel.spawn("cleanup-mutex-waiter",looping);
    assert(waiterCleanupKernel.mutexLock(cleanupOwner,cleanupMutex));
    assert(!waiterCleanupKernel.mutexLock(cleanupBlocked,cleanupMutex));
    assert(waiterCleanupKernel.terminateProcess(cleanupBlocked));
    assert(waiterCleanupKernel.syncDiagnostics().find("waiters=[]")!=std::string::npos);
    assert(waiterCleanupKernel.mutexUnlock(cleanupOwner,cleanupMutex));

    // TERM/KILL signal paths must perform the same synchronization cleanup.
    const auto signalSemaphore=waiterCleanupKernel.createSemaphore(0,1);
    const auto signalDeadWaiter=waiterCleanupKernel.spawn("signal-dead-sem-waiter",looping);
    const auto signalLiveWaiter=waiterCleanupKernel.spawn("signal-live-sem-waiter",looping);
    assert(!waiterCleanupKernel.semaphoreWait(signalDeadWaiter,signalSemaphore));
    assert(!waiterCleanupKernel.semaphoreWait(signalLiveWaiter,signalSemaphore));
    assert(waiterCleanupKernel.sendSignal(signalDeadWaiter,nexa::ProcessSignal::Kill));
    assert(waiterCleanupKernel.semaphoreSignal(signalSemaphore));
    assert(waiterCleanupKernel.semaphoreWait(signalLiveWaiter,signalSemaphore));
    const auto termMutex=waiterCleanupKernel.createMutex();
    const auto termOwner=waiterCleanupKernel.spawn("term-mutex-owner",looping);
    const auto termWaiter=waiterCleanupKernel.spawn("term-mutex-waiter",looping);
    assert(waiterCleanupKernel.mutexLock(termOwner,termMutex));
    assert(!waiterCleanupKernel.mutexLock(termWaiter,termMutex));
    assert(waiterCleanupKernel.sendSignal(termWaiter,nexa::ProcessSignal::Terminate));
    assert(waiterCleanupKernel.syncDiagnostics().find("waiters=[]")!=std::string::npos);
    assert(waiterCleanupKernel.mutexUnlock(termOwner,termMutex));

    // Guest syscalls attach, read, write and detach a kernel-created segment.
    const auto guestSegment=syncKernel.createSharedMemory(16);
    assert(syncKernel.attachSharedMemory(ownerPid,guestSegment));
    auto attachmentStats=syncKernel.processes();
    const auto ownerStats=std::find_if(attachmentStats.begin(),attachmentStats.end(),[&](const auto& process){return process.pid==ownerPid;});
    assert(ownerStats!=attachmentStats.end()&&ownerStats->sharedMemoryAttachments==1);
    assert(syncKernel.writeSharedMemory(ownerPid,guestSegment,4,0x33));
    assert(syncKernel.writeSharedMemory(ownerPid,guestSegment,8,0x10));
    assert(syncKernel.writeSharedMemory(ownerPid,guestSegment,9,0xff));
    assert(syncKernel.sharedAtomicAdd(ownerPid,guestSegment,9,2)==0xff);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,9)==1);
    assert(syncKernel.sharedCompareSwap(ownerPid,guestSegment,9,1,7)==1);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,9)==7);
    // 16-bit little-endian atomic operations, including wraparound.
    assert(syncKernel.writeSharedMemory(ownerPid,guestSegment,10,0xfe));
    assert(syncKernel.writeSharedMemory(ownerPid,guestSegment,11,0xff));
    assert(syncKernel.sharedAtomicAdd16(ownerPid,guestSegment,10,3)==0xfffe);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,10)==1);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,11)==0);
    assert(syncKernel.sharedCompareSwap16(ownerPid,guestSegment,10,1,0x1234)==1);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,10)==0x34);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,11)==0x12);
    assert(syncKernel.sharedCompareSwap16(ownerPid,guestSegment,10,1,0x5678)==0x1234);
    // Guest ABI returns the low byte in A and writes the high byte beside the descriptor.
    syncKernel.writeProcessMemory(ownerPid,200,guestSegment);
    syncKernel.writeProcessMemory(ownerPid,201,10);
    syncKernel.writeProcessMemory(ownerPid,202,1);
    syncKernel.writeProcessMemory(ownerPid,203,0);
    syncKernel.writeProcessMemory(ownerPid,204,0);
    assert(syncKernel.syscall(ownerPid,nexa::Syscall::SharedAtomicAdd16,200)==0x34);
    assert(syncKernel.readProcessMemory(ownerPid,204)==0x12);
    syncKernel.writeProcessMemory(ownerPid,210,guestSegment);
    syncKernel.writeProcessMemory(ownerPid,211,10);
    syncKernel.writeProcessMemory(ownerPid,212,0x35);
    syncKernel.writeProcessMemory(ownerPid,213,0x12);
    syncKernel.writeProcessMemory(ownerPid,214,0xcd);
    syncKernel.writeProcessMemory(ownerPid,215,0xab);
    syncKernel.writeProcessMemory(ownerPid,216,0);
    assert(syncKernel.syscall(ownerPid,nexa::Syscall::SharedCompareSwap16,210)==0x35);
    assert(syncKernel.readProcessMemory(ownerPid,216)==0x12);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,10)==0xcd);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,11)==0xab);
    assert(!syncKernel.sharedAtomicAdd16(ownerPid,guestSegment,15,1)); // segment boundary

    assert(syncKernel.sharedCompareSwap(ownerPid,guestSegment,9,1,8)==7);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,9)==7);
    const auto shmGuestPid=syncKernel.spawn("guest-shm-client",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,guestSegment},{nexa::Opcode::Store,40},{nexa::Opcode::LoadImmediate,10},{nexa::Opcode::Store,41},{nexa::Opcode::LoadBImmediate,40},{nexa::Opcode::LoadImmediate,static_cast<std::uint8_t>(nexa::Syscall::SharedAttach)},{nexa::Opcode::Syscall,0},{nexa::Opcode::Store,48},
        {nexa::Opcode::LoadImmediate,guestSegment},{nexa::Opcode::Store,40},{nexa::Opcode::LoadImmediate,4},{nexa::Opcode::Store,41},
        {nexa::Opcode::LoadBImmediate,40},{nexa::Opcode::LoadImmediate,static_cast<std::uint8_t>(nexa::Syscall::SharedRead)},{nexa::Opcode::Syscall,0},{nexa::Opcode::Store,49},
        {nexa::Opcode::LoadImmediate,guestSegment},{nexa::Opcode::Store,40},{nexa::Opcode::LoadImmediate,5},{nexa::Opcode::Store,41},{nexa::Opcode::LoadImmediate,0x77},{nexa::Opcode::Store,42},
        {nexa::Opcode::LoadBImmediate,40},{nexa::Opcode::LoadImmediate,static_cast<std::uint8_t>(nexa::Syscall::SharedWrite)},{nexa::Opcode::Syscall,0},{nexa::Opcode::Store,44},
        {nexa::Opcode::LoadImmediate,guestSegment},{nexa::Opcode::Store,40},{nexa::Opcode::LoadImmediate,8},{nexa::Opcode::Store,41},{nexa::Opcode::LoadImmediate,3},{nexa::Opcode::Store,42},{nexa::Opcode::LoadBImmediate,40},{nexa::Opcode::LoadImmediate,static_cast<std::uint8_t>(nexa::Syscall::SharedAtomicAdd)},{nexa::Opcode::Syscall,0},{nexa::Opcode::Store,46},
        {nexa::Opcode::LoadImmediate,guestSegment},{nexa::Opcode::Store,40},{nexa::Opcode::LoadImmediate,8},{nexa::Opcode::Store,41},{nexa::Opcode::LoadImmediate,0x13},{nexa::Opcode::Store,42},{nexa::Opcode::LoadImmediate,0x44},{nexa::Opcode::Store,43},{nexa::Opcode::LoadBImmediate,40},{nexa::Opcode::LoadImmediate,static_cast<std::uint8_t>(nexa::Syscall::SharedCompareSwap)},{nexa::Opcode::Syscall,0},{nexa::Opcode::Store,47},
        {nexa::Opcode::LoadImmediate,0x66},{nexa::Opcode::Store,2566},{nexa::Opcode::Load,2566},{nexa::Opcode::Store,45},
        {nexa::Opcode::Halt,0}}});    syncKernel.setPriority(shmGuestPid,7);
    for(int retry=0;retry<100;++retry) {
        const auto states=syncKernel.processes();
        const auto guestState=std::find_if(states.begin(),states.end(),[&](const auto& process){return process.pid==shmGuestPid;});
        if(guestState!=states.end()&&guestState->state==nexa::ProcessState::Halted)break;
        assert(syncKernel.scheduleOne());
    }
    const auto guestFinished=syncKernel.processes();
    const auto finishedGuest=std::find_if(guestFinished.begin(),guestFinished.end(),[&](const auto& process){return process.pid==shmGuestPid;});
    assert(finishedGuest!=guestFinished.end()&&finishedGuest->state==nexa::ProcessState::Halted);
    assert(syncKernel.readProcessMemory(shmGuestPid,48)==1);
    assert(!syncKernel.memoryManager().isMapped(shmGuestPid,10)); // Halt auto-detaches guest mappings
    assert(syncKernel.sharedMemoryInfo().front().attachments==1);
    assert(syncKernel.readProcessMemory(shmGuestPid,49)==0x33);
    assert(syncKernel.readProcessMemory(shmGuestPid,44)==1);
    assert(syncKernel.readProcessMemory(shmGuestPid,45)==0x66);
    assert(syncKernel.readProcessMemory(shmGuestPid,46)==0x10);
    assert(syncKernel.readProcessMemory(shmGuestPid,47)==0x13);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,8)==0x44);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,4)==0x33);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,5)==0x77);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,6)==0x66);
    assert(syncKernel.attachSharedMemory(lockWaiterPid,guestSegment));
    assert(syncKernel.mapSharedMemory(lockWaiterPid,guestSegment,3));
    syncKernel.memoryManager().write(lockWaiterPid,static_cast<std::uint16_t>(3*256+7),0x99,nexa::MemoryAccess::User);
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,7)==0x99);
    bool sharedExecuteDenied=false;
    try { (void)syncKernel.memoryManager().canExecute(lockWaiterPid,static_cast<std::uint16_t>(3*256),nexa::MemoryAccess::User); }
    catch(const std::runtime_error&) { sharedExecuteDenied=true; }
    assert(sharedExecuteDenied);
    // A read-only mapping permits reads but rejects both guest-page and kernel shared writes.
    assert(syncKernel.unmapSharedMemory(lockWaiterPid,guestSegment));
    assert(syncKernel.mapSharedMemory(lockWaiterPid,guestSegment,3,false));
    assert(syncKernel.memoryManager().read(lockWaiterPid,static_cast<std::uint16_t>(3*256+7),nexa::MemoryAccess::User)==0x99);
    bool readOnlyStoreDenied=false;
    try { syncKernel.memoryManager().write(lockWaiterPid,static_cast<std::uint16_t>(3*256+7),0x55,nexa::MemoryAccess::User); }
    catch(const std::runtime_error&) { readOnlyStoreDenied=true; }
    assert(readOnlyStoreDenied);
    assert(!syncKernel.writeSharedMemory(lockWaiterPid,guestSegment,7,0x55));
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,7)==0x99);
    assert(syncKernel.detachSharedMemory(lockWaiterPid,guestSegment));
    attachmentStats=syncKernel.processes();
    const auto detachedStats=std::find_if(attachmentStats.begin(),attachmentStats.end(),[&](const auto& process){return process.pid==lockWaiterPid;});
    assert(detachedStats!=attachmentStats.end()&&detachedStats->sharedMemoryAttachments==0);
    assert(!syncKernel.memoryManager().isMapped(lockWaiterPid,3));
    assert(!syncKernel.detachSharedMemory(shmGuestPid,guestSegment)); // Guest already detached on exit
    assert(!syncKernel.memoryManager().isMapped(shmGuestPid,2));
    assert(syncKernel.readSharedMemory(ownerPid,guestSegment,5)==0x77);
    assert(!syncKernel.readSharedMemory(shmGuestPid,guestSegment,4));
    assert(syncKernel.destroySharedMemory(guestSegment));
    // Guest can allocate a shared segment through syscall 22.
    nexa::NexaKernel createKernel;
    const auto createPid=createKernel.spawn("guest-shm-create",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,16},{nexa::Opcode::Store,50},{nexa::Opcode::LoadImmediate,0},{nexa::Opcode::Store,51},
        {nexa::Opcode::LoadBImmediate,50},{nexa::Opcode::LoadImmediate,static_cast<std::uint8_t>(nexa::Syscall::SharedCreate)},{nexa::Opcode::Syscall,0},{nexa::Opcode::Store,52},{nexa::Opcode::Halt,0}}});
    createKernel.run();
    const auto createdSegment=createKernel.readProcessMemory(createPid,52);
    assert(createdSegment!=0);
    const auto createdInfo=createKernel.sharedMemoryInfo();
    assert(createdInfo.size()==1&&createdInfo.front().id==createdSegment&&createdInfo.front().size==16);
    assert(createKernel.destroySharedMemory(createdSegment));
    // Kernel-managed shared-memory segment API: attachment permissions and cleanup.
    const auto sharedId=syncKernel.createSharedMemory(32);
    assert(syncKernel.attachSharedMemory(ownerPid,sharedId));
    assert(syncKernel.attachSharedMemory(lockWaiterPid,sharedId));
    assert(syncKernel.writeSharedMemory(ownerPid,sharedId,7,0xA5));
    assert(syncKernel.readSharedMemory(lockWaiterPid,sharedId,7)==0xA5);
    assert(!syncKernel.readSharedMemory(shmGuestPid,sharedId,7));
    assert(!syncKernel.writeSharedMemory(ownerPid,sharedId,32,1));
    assert(syncKernel.detachSharedMemory(ownerPid,sharedId));
    assert(!syncKernel.readSharedMemory(ownerPid,sharedId,7));
    assert(syncKernel.sharedMemoryInfo().front().attachments==1);
    assert(syncKernel.destroySharedMemory(sharedId));
    assert(syncKernel.sharedMemoryInfo().empty());
    const auto cleanupSegment=syncKernel.createSharedMemory(8);
    const auto cleanupPid=syncKernel.spawn("shared-attachment-cleanup",looping);
    assert(syncKernel.attachSharedMemory(cleanupPid,cleanupSegment));
    assert(syncKernel.sharedMemoryInfo().front().attachments==1);
    assert(syncKernel.terminateProcess(cleanupPid));
    assert(syncKernel.sharedMemoryInfo().front().attachments==0);
    assert(syncKernel.destroySharedMemory(cleanupSegment));
    bool invalidSharedSize=false;try{(void)syncKernel.createSharedMemory(0);}catch(const std::invalid_argument&){invalidSharedSize=true;}assert(invalidSharedSize);
    // Repeated multi-process shared-memory lifecycle stress: aliases must be
    // removed and every physical frame returned after each destroy.
    nexa::NexaKernel sharedStressKernel(2048);
    const auto stressA=sharedStressKernel.spawn("shm-stress-a",looping);
    const auto stressB=sharedStressKernel.spawn("shm-stress-b",looping);
    const auto stressFreeFrames=sharedStressKernel.memoryManager().freeFrames();
    for(int cycle=0;cycle<40;++cycle) {
        const auto stressId=sharedStressKernel.createSharedMemory(300); // two frames
        assert(sharedStressKernel.memoryManager().freeFrames()==stressFreeFrames-2);
        assert(sharedStressKernel.attachSharedMemory(stressA,stressId));
        assert(sharedStressKernel.attachSharedMemory(stressB,stressId));
        const auto baseA=static_cast<std::uint8_t>(20+(cycle%20)*2);
        const auto baseB=static_cast<std::uint8_t>(70+(cycle%20)*2);
        assert(sharedStressKernel.mapSharedMemory(stressA,stressId,baseA));
        assert(sharedStressKernel.mapSharedMemory(stressB,stressId,baseB));
        assert(sharedStressKernel.writeSharedMemory(stressA,stressId,299,static_cast<std::uint8_t>(cycle)));
        assert(sharedStressKernel.readSharedMemory(stressB,stressId,299)==static_cast<std::uint8_t>(cycle));
        assert(sharedStressKernel.unmapSharedMemory(stressA,stressId));
        assert(sharedStressKernel.unmapSharedMemory(stressB,stressId));
        assert(!sharedStressKernel.memoryManager().isMapped(stressA,baseA));
        assert(!sharedStressKernel.memoryManager().isMapped(stressB,baseB));
        assert(sharedStressKernel.detachSharedMemory(stressA,stressId));
        assert(sharedStressKernel.detachSharedMemory(stressB,stressId));
        assert(sharedStressKernel.destroySharedMemory(stressId));
        assert(sharedStressKernel.sharedMemoryInfo().empty());
        assert(sharedStressKernel.memoryManager().freeFrames()==stressFreeFrames);
    }

    // A late-page collision must roll back earlier aliases without touching
    // the guest's pre-existing mapping.
    nexa::NexaKernel rollbackKernel(1024);
    const auto rollbackPid=rollbackKernel.spawn("shm-map-rollback",looping);
    const auto rollbackFree=rollbackKernel.memoryManager().freeFrames();
    const auto rollbackId=rollbackKernel.createSharedMemory(300);
    assert(rollbackKernel.attachSharedMemory(rollbackPid,rollbackId));
    rollbackKernel.memoryManager().mapPage(rollbackPid,101,true,true,false);
    const bool mappingCollision=rollbackKernel.mapSharedMemory(rollbackPid,rollbackId,100);
    assert(!mappingCollision);
    assert(!rollbackKernel.memoryManager().isMapped(rollbackPid,100));
    assert(rollbackKernel.memoryManager().isMapped(rollbackPid,101));
    assert(rollbackKernel.memoryManager().mappedPages(rollbackPid)>=1);
    rollbackKernel.memoryManager().unmapPage(rollbackPid,101);
    assert(rollbackKernel.destroySharedMemory(rollbackId));
    assert(rollbackKernel.memoryManager().freeFrames()==rollbackFree);

    // Destroying a live mapped segment must revoke every guest alias and
    // reclaim its backing frames even while the process remains alive.
    const auto revokeId=rollbackKernel.createSharedMemory(300);
    assert(rollbackKernel.attachSharedMemory(rollbackPid,revokeId));
    assert(rollbackKernel.mapSharedMemory(rollbackPid,revokeId,110));
    assert(rollbackKernel.memoryManager().isMapped(rollbackPid,110));
    assert(rollbackKernel.memoryManager().isMapped(rollbackPid,111));
    assert(rollbackKernel.destroySharedMemory(revokeId));
    assert(!rollbackKernel.memoryManager().isMapped(rollbackPid,110));
    assert(!rollbackKernel.memoryManager().isMapped(rollbackPid,111));
    assert(!rollbackKernel.readSharedMemory(rollbackPid,revokeId,0));
    assert(rollbackKernel.memoryManager().freeFrames()==rollbackFree);

    const auto mutex2=syncKernel.createMutex();
    assert(syncKernel.mutexLock(ownerPid,mutex2));
    assert(!syncKernel.mutexLock(ownerPid,mutex2));
    assert(!syncKernel.mutexUnlock(lockWaiterPid,mutex2));
    
    nexa::NexaKernel signalKernel(256);
    const auto signalPid=signalKernel.spawn("signal-target",looping);
    assert(signalKernel.sendSignal(signalPid,nexa::ProcessSignal::Stop));
    auto signalProcesses=signalKernel.processes();
    assert(signalProcesses.size()==1&&signalProcesses.front().state==nexa::ProcessState::Stopped);
    assert(!signalKernel.scheduleOne());
    assert(signalKernel.sendSignal(signalPid,nexa::ProcessSignal::User1));
    assert(signalKernel.sendSignal(signalPid,nexa::ProcessSignal::User2));
    signalProcesses=signalKernel.processes();
    assert(signalProcesses.front().pendingSignals==2);
    assert(signalKernel.sendSignal(signalPid,nexa::ProcessSignal::Continue));
    assert(signalKernel.scheduleOne());
    auto deliveredSignal=signalKernel.receiveSignal(signalPid);
    assert(deliveredSignal&&*deliveredSignal==nexa::ProcessSignal::User1);
    deliveredSignal=signalKernel.receiveSignal(signalPid);
    assert(deliveredSignal&&*deliveredSignal==nexa::ProcessSignal::User2);
    assert(!signalKernel.receiveSignal(signalPid));
    assert(!signalKernel.sendSignal(signalPid,nexa::ProcessSignal::Continue));
    for(std::size_t n=0;n<32;++n) assert(signalKernel.sendSignal(signalPid,nexa::ProcessSignal::User1));
    assert(!signalKernel.sendSignal(signalPid,nexa::ProcessSignal::User2));
    signalProcesses=signalKernel.processes();
    assert(signalProcesses.front().pendingSignals==32);
    const auto signalGuestPid=signalKernel.spawn("signal-guest",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(nexa::Syscall::GetSignal)},
        {nexa::Opcode::Syscall,0},{nexa::Opcode::Store,90},{nexa::Opcode::Halt,0}}});
    assert(signalKernel.sendSignal(signalGuestPid,nexa::ProcessSignal::User2));
    signalKernel.run(20);
    assert(signalKernel.readProcessMemory(signalGuestPid,90)==static_cast<std::uint8_t>(nexa::ProcessSignal::User2));
    const auto termPid=signalKernel.spawn("term-target",looping);
    assert(signalKernel.sendSignal(termPid,nexa::ProcessSignal::Terminate));
    signalProcesses=signalKernel.processes();
    bool termRecorded=false;
    for(const auto& proc:signalProcesses) if(proc.pid==termPid)
        termRecorded=proc.state==nexa::ProcessState::Halted&&proc.exitCode==143&&proc.terminationReason=="signal:TERM";
    assert(termRecorded&&!signalKernel.sendSignal(termPid,nexa::ProcessSignal::Continue));
    const auto killSignalPid=signalKernel.spawn("kill-signal-target",looping);
    assert(signalKernel.sendSignal(killSignalPid,nexa::ProcessSignal::Kill));
    bool killSignalRecorded=false;
    for(const auto& proc:signalKernel.processes()) if(proc.pid==killSignalPid)
        killSignalRecorded=proc.state==nexa::ProcessState::Halted&&proc.exitCode==137&&proc.terminationReason=="signal:KILL";
    assert(killSignalRecorded);

    nexa::NexaKernel signalShellKernel(256);
    const auto shellSignalPid=signalShellKernel.spawn("shell-signal-target",looping);
    nexa::NexaShell signalShell(signalShellKernel);
    assert(signalShell.execute("signal "+std::to_string(shellSignalPid)+" STOP")=="signal delivered");
    assert(signalShell.execute("ps").find("stopped")!=std::string::npos);
    assert(signalShell.execute("signal "+std::to_string(shellSignalPid)+" CONT")=="signal delivered");
    assert(signalShell.execute("signal "+std::to_string(shellSignalPid)+" USR1")=="signal delivered");
    assert(signalShell.execute("ps").find("signals=1")!=std::string::npos);
    assert(signalShell.execute("signal "+std::to_string(shellSignalPid)+" TERM")=="signal delivered");
    assert(signalShell.execute("signal "+std::to_string(shellSignalPid)+" BAD")=="invalid signal (TERM, KILL, STOP, CONT, USR1, USR2)");

    nexa::NexaKernel exitCleanupKernel(256);
    const auto exitCleanupPid=exitCleanupKernel.spawn("exit-cleanup",nexa::Executable{0,{{nexa::Opcode::Halt,0}}});
    exitCleanupKernel.filesystem().createFile("/exit-data",{44});
    exitCleanupKernel.grantFileAccess(exitCleanupPid,"/exit-data",nexa::FilePermission::Read);
    const std::string exitPath="/exit-data";
    for(std::size_t index=0;index<exitPath.size();++index)
        exitCleanupKernel.writeProcessMemory(exitCleanupPid,static_cast<std::uint16_t>(40+index),static_cast<std::uint8_t>(exitPath[index]));
    exitCleanupKernel.writeProcessMemory(exitCleanupPid,static_cast<std::uint16_t>(40+exitPath.size()),0);
    const auto exitHandle=exitCleanupKernel.syscall(exitCleanupPid,nexa::Syscall::OpenRead,40);
    assert(exitHandle!=0);
    auto resourceStats=exitCleanupKernel.processes();
    assert(resourceStats.size()==1&&resourceStats.front().openFileHandles==1);
    assert(exitCleanupKernel.syscall(exitCleanupPid,nexa::Syscall::Exit,23)==0);
    resourceStats=exitCleanupKernel.processes();
    assert(resourceStats.size()==1&&resourceStats.front().openFileHandles==0&&resourceStats.front().sharedMemoryAttachments==0);
    bool exitHandleClosed=false;
    try{(void)exitCleanupKernel.syscall(exitCleanupPid,nexa::Syscall::ReadByte,exitHandle);}
    catch(const std::runtime_error&){exitHandleClosed=true;}
    assert(exitHandleClosed);
    auto exitProcesses=exitCleanupKernel.processes();
    assert(exitProcesses.size()==1&&exitProcesses.front().exitCode==23&&exitProcesses.front().terminationReason=="exit");

    nexa::NexaKernel ipcKernel(256);
    const auto ipcSender=ipcKernel.spawn("ipc-host-sender",nexa::Executable{0,{{nexa::Opcode::Halt,0}}});
    const auto ipcReceiver=ipcKernel.spawn("ipc-host-receiver",nexa::Executable{0,{{nexa::Opcode::Halt,0}}});
    assert(ipcKernel.sendMessage(ipcSender,ipcReceiver,{1,2,3}));
    auto ipcStats=ipcKernel.processes();
    for(const auto& proc:ipcStats) if(proc.pid==ipcReceiver) { assert(proc.pendingMessages==1); assert(proc.pendingMessageBytes==3); }
    auto ipcMessage=ipcKernel.receiveMessage(ipcReceiver);
    assert(ipcMessage&&ipcMessage->sender==ipcSender&&ipcMessage->payload==std::vector<std::uint8_t>({1,2,3}));
    assert(!ipcKernel.receiveMessage(ipcReceiver));
    ipcStats=ipcKernel.processes();
    for(const auto& proc:ipcStats) if(proc.pid==ipcReceiver) { assert(proc.pendingMessages==0); assert(proc.pendingMessageBytes==0); }
    assert(ipcKernel.sendMessage(ipcSender,ipcReceiver,{4,5}));
    assert(ipcKernel.sendMessage(ipcSender,ipcReceiver,{6,7,8,9}));
    ipcStats=ipcKernel.processes();
    for(const auto& proc:ipcStats) if(proc.pid==ipcReceiver) { assert(proc.pendingMessages==2); assert(proc.pendingMessageBytes==6); }
    auto fifoFirst=ipcKernel.receiveMessage(ipcReceiver);
    assert(fifoFirst&&fifoFirst->payload==std::vector<std::uint8_t>({4,5}));
    ipcStats=ipcKernel.processes();
    for(const auto& proc:ipcStats) if(proc.pid==ipcReceiver) { assert(proc.pendingMessages==1); assert(proc.pendingMessageBytes==4); }
    assert(ipcKernel.receiveMessage(ipcReceiver)->payload==std::vector<std::uint8_t>({6,7,8,9}));
    std::vector<std::uint8_t> quotaPayload(nexa::NexaKernel::MaxIpcPayload,0x5A);
    for(std::size_t n=0;n<32;++n) assert(ipcKernel.sendMessage(ipcSender,ipcReceiver,quotaPayload));
    ipcStats=ipcKernel.processes();
    for(const auto& proc:ipcStats) if(proc.pid==ipcReceiver) { assert(proc.pendingMessages==32); assert(proc.pendingMessageBytes==32*nexa::NexaKernel::MaxIpcPayload); }
    assert(!ipcKernel.sendMessage(ipcSender,ipcReceiver,quotaPayload));
    assert(ipcKernel.receiveMessage(ipcReceiver).has_value());
    assert(ipcKernel.sendMessage(ipcSender,ipcReceiver,quotaPayload));
    ipcStats=ipcKernel.processes();
    for(const auto& proc:ipcStats) if(proc.pid==ipcReceiver) { assert(proc.pendingMessages==32); assert(proc.pendingMessageBytes==32*nexa::NexaKernel::MaxIpcPayload); }
    std::vector<std::uint8_t> oversized(252,0);
    assert(!ipcKernel.sendMessage(ipcSender,ipcReceiver,std::move(oversized)));
    // Leave the shared test kernel empty for the following 64-message FIFO test.
    for(std::size_t n=0;n<32;++n) assert(ipcKernel.receiveMessage(ipcReceiver).has_value());
    assert(!ipcKernel.receiveMessage(ipcReceiver));

    nexa::NexaKernel guestIpcKernel(256);
    const auto guestReceiver=guestIpcKernel.spawn("guest-receiver",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(nexa::Syscall::ReceiveMessage)},
        {nexa::Opcode::LoadBImmediate,100},{nexa::Opcode::Syscall,0},
        {nexa::Opcode::Load,105},{nexa::Opcode::Output,nexa::VM::DisplayPort},
        {nexa::Opcode::Halt,0}}});
    const auto guestSender=guestIpcKernel.spawn("guest-sender",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(guestReceiver&0xFF)},{nexa::Opcode::Store,100},
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>((guestReceiver>>8)&0xFF)},{nexa::Opcode::Store,101},
        {nexa::Opcode::LoadImmediate,1},{nexa::Opcode::Store,102},
        {nexa::Opcode::LoadImmediate,'Z'},{nexa::Opcode::Store,103},
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(nexa::Syscall::SendMessage)},
        {nexa::Opcode::LoadBImmediate,100},{nexa::Opcode::Syscall,0},{nexa::Opcode::Halt,0}}});
    guestIpcKernel.setPriority(guestSender,1);
    guestIpcKernel.setPriority(guestReceiver,7);
    guestIpcKernel.setTimeQuantum(32);
    assert(guestIpcKernel.scheduleOne());
    auto guestIpcProcesses=guestIpcKernel.processes();
    bool receiverBlocked=false;
    for(const auto& proc:guestIpcProcesses)if(proc.pid==guestReceiver)receiverBlocked=proc.state==nexa::ProcessState::Blocked;
    assert(receiverBlocked&&!guestIpcKernel.hasDeadlock());
    assert(guestIpcKernel.scheduleOne());
    guestIpcProcesses=guestIpcKernel.processes();
    bool receiverWoken=false;
    for(const auto& proc:guestIpcProcesses)if(proc.pid==guestReceiver)receiverWoken=proc.state==nexa::ProcessState::Ready;
    assert(receiverWoken);
    bool guestMessageQueued=false;
    for(const auto& proc:guestIpcProcesses)if(proc.pid==guestReceiver)guestMessageQueued=proc.pendingMessages==1;
    assert(guestMessageQueued);
    guestIpcKernel.run();
    guestIpcProcesses=guestIpcKernel.processes();
    for(const auto& proc:guestIpcProcesses)
        if(proc.pid==guestReceiver&&proc.state==nexa::ProcessState::Faulted)
            throw std::runtime_error("guest IPC receiver fault: "+proc.faultReason);
    const auto guestIpcOutput=guestIpcKernel.takeConsoleOutput();
    if(guestIpcOutput!="Z") {
        std::string diagnostic="guest IPC output mismatch; bytes="+std::to_string(guestIpcOutput.size());
        for(const auto& proc:guestIpcProcesses)
            diagnostic+=" pid="+std::to_string(proc.pid)+" state="+std::to_string(static_cast<int>(proc.state))+
                " ticks="+std::to_string(proc.ticks)+" inbox="+std::to_string(proc.pendingMessages)+
                " fault="+proc.faultReason;
        throw std::runtime_error(diagnostic);
    }

    nexa::NexaKernel unavailableIpcKernel(256);
    const auto unavailableSender=unavailableIpcKernel.spawn("unavailable-sender",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,0xFF},{nexa::Opcode::Store,100},
        {nexa::Opcode::LoadImmediate,0xFF},{nexa::Opcode::Store,101},
        {nexa::Opcode::LoadImmediate,0},{nexa::Opcode::Store,102},
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(nexa::Syscall::SendMessage)},
        {nexa::Opcode::LoadBImmediate,100},{nexa::Opcode::Syscall,0},
        {nexa::Opcode::Store,104},{nexa::Opcode::Halt,0}}});
    unavailableIpcKernel.run();
    assert(unavailableIpcKernel.readProcessMemory(unavailableSender,104)==0);
    bool unavailableSenderHalted=false;
    for(const auto& proc:unavailableIpcKernel.processes())
        if(proc.pid==unavailableSender) unavailableSenderHalted=proc.state==nexa::ProcessState::Halted;
    assert(unavailableSenderHalted);

    nexa::NexaKernel blockedKernel(256);
    const auto waitingPid=blockedKernel.spawn("waiting-reader",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(nexa::Syscall::ReceiveMessage)},
        {nexa::Opcode::LoadBImmediate,100},{nexa::Opcode::Syscall,0},
        {nexa::Opcode::Load,105},{nexa::Opcode::Output,nexa::VM::DisplayPort},
        {nexa::Opcode::Halt,0}}});
    blockedKernel.setTimeQuantum(8);
    assert(blockedKernel.scheduleOne());
    assert(blockedKernel.hasDeadlock());
    assert(!blockedKernel.scheduleOne());
    const auto wakeSource=blockedKernel.spawn("wake-source",nexa::Executable{0,{{nexa::Opcode::Halt,0}}});
    assert(!blockedKernel.hasDeadlock());
    assert(blockedKernel.sendMessage(wakeSource,waitingPid,{'A'}));
    assert(blockedKernel.scheduleOne());
    blockedKernel.run();
    assert(blockedKernel.takeConsoleOutput()=="A");

    // Mutex wait-for graph distinguishes a circular wait from a single blocked waiter.
    nexa::NexaKernel mutexDeadlockKernel(256);
    const auto cycleA=mutexDeadlockKernel.spawn("cycle-a",nexa::Executable{0,{{nexa::Opcode::Halt,0}}});
    const auto cycleB=mutexDeadlockKernel.spawn("cycle-b",nexa::Executable{0,{{nexa::Opcode::Halt,0}}});
    const auto cycleMutexA=mutexDeadlockKernel.createMutex();
    const auto cycleMutexB=mutexDeadlockKernel.createMutex();
    assert(mutexDeadlockKernel.mutexLock(cycleA,cycleMutexA));
    assert(mutexDeadlockKernel.mutexLock(cycleB,cycleMutexB));
    assert(!mutexDeadlockKernel.mutexLock(cycleA,cycleMutexB));
    assert(!mutexDeadlockKernel.hasMutexDeadlock());
    assert(!mutexDeadlockKernel.mutexLock(cycleB,cycleMutexA));
    assert(mutexDeadlockKernel.hasMutexDeadlock());
    const auto detectedCycle=mutexDeadlockKernel.mutexDeadlockCycle();
    assert(detectedCycle.size()==2);
    assert(std::find(detectedCycle.begin(),detectedCycle.end(),cycleA)!=detectedCycle.end());
    assert(std::find(detectedCycle.begin(),detectedCycle.end(),cycleB)!=detectedCycle.end());
    // Owner exit removes its wait edge and hands its mutex to the surviving waiter.
    assert(mutexDeadlockKernel.terminateProcess(cycleB));
    assert(!mutexDeadlockKernel.hasMutexDeadlock());
    assert(mutexDeadlockKernel.mutexDeadlockCycle().empty());
    assert(mutexDeadlockKernel.mutexLock(cycleA,cycleMutexB));

    // Inbox capacity is bounded and FIFO; a full mailbox never overwrites messages.
    for(std::uint8_t value=0;value<64;++value)
        assert(ipcKernel.sendMessage(ipcSender,ipcReceiver,{value}));
    assert(!ipcKernel.sendMessage(ipcSender,ipcReceiver,{99}));
    auto ipcProcesses=ipcKernel.processes();
    bool inboxFull=false;
    for(const auto& proc:ipcProcesses)if(proc.pid==ipcReceiver)inboxFull=proc.pendingMessages==64;
    assert(inboxFull);
    for(std::uint8_t value=0;value<64;++value) {
        auto message=ipcKernel.receiveMessage(ipcReceiver);
        assert(message&&message->payload.size()==1&&message->payload[0]==value);
    }


    nexa::NexaKernel pagingKernel(1024);
    const auto pagingPid=pagingKernel.spawn("paged-load-store",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,90},{nexa::Opcode::Store,200},
        {nexa::Opcode::Load,200},{nexa::Opcode::Output,nexa::VM::DisplayPort},
        {nexa::Opcode::Halt,0}}});
    assert(pagingKernel.memoryManager().mappedPages(pagingPid)==0);
    auto pagingStats=pagingKernel.processes();
    for(const auto& process:pagingStats) if(process.pid==pagingPid) {
        assert(process.virtualPages==1);
        assert(process.residentPages==1);
        assert(process.swappedPages==0);
        assert(process.residentBytes==nexa::VirtualMemoryManager::PageSize);
    }
    pagingKernel.run();
    assert(pagingKernel.memoryManager().read(pagingPid,200,nexa::MemoryAccess::Kernel)==90);
    assert(pagingKernel.memoryManager().mappedPages(pagingPid)==1);
    pagingStats=pagingKernel.processes();
    for(const auto& process:pagingStats) if(process.pid==pagingPid) {
        assert(process.virtualPages==2);
        assert(process.residentPages==2);
        assert(process.swappedPages==0);
        assert(process.residentBytes==2*nexa::VirtualMemoryManager::PageSize);
    }
    assert(pagingKernel.memoryManager().pageFaults()>=1);
    assert(pagingKernel.takeConsoleOutput()=="Z");
    const auto executeFaultPid=pagingKernel.spawn("execute-protection",nexa::Executable{0,{
        {nexa::Opcode::Halt,0}}});
    const auto executeSpace=executeFaultPid|0x80000000u;
    assert(pagingKernel.memoryManager().canExecute(executeSpace,0));
    assert(pagingKernel.memoryManager().read(executeSpace,0,nexa::MemoryAccess::Kernel)==static_cast<std::uint8_t>(nexa::Opcode::Halt));
    bool codeWriteDenied=false;
    try { pagingKernel.memoryManager().write(executeSpace,0,0xFF); } catch(const std::runtime_error&) { codeWriteDenied=true; }
    assert(codeWriteDenied);
    pagingKernel.memoryManager().protectPage(executeSpace,0,false,true,false);
    pagingKernel.run();
    bool executeFault=false;
    for(const auto& p:pagingKernel.processes())
        if(p.pid==executeFaultPid) executeFault=p.state==nexa::ProcessState::Faulted&&p.faultReason.find("execute protection")!=std::string::npos;
    assert(executeFault);
    const auto protectedPid=pagingKernel.spawn("protected-store",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,1},{nexa::Opcode::Store,0},{nexa::Opcode::Halt,0}}});
    pagingKernel.memoryManager().mapPage(protectedPid,0,true,false,false);
    pagingKernel.memoryManager().protectPage(protectedPid,0,true,false,false);
    pagingKernel.run();
    bool protectedFault=false;
    for(const auto& p:pagingKernel.processes())
        if(p.pid==protectedPid) protectedFault=p.state==nexa::ProcessState::Faulted&&p.faultReason.find("supervisor-only")!=std::string::npos;
    assert(protectedFault);
    nexa::NexaKernel reclaimKernel(1024);
    const auto freeBefore=reclaimKernel.memoryManager().freeFrames();
    const auto reclaimPid=reclaimKernel.spawn("reclaim",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,7},{nexa::Opcode::Store,9},{nexa::Opcode::Halt,0}}});
    reclaimKernel.run();
    assert(reclaimKernel.memoryManager().freeFrames()<freeBefore);
    assert(reclaimKernel.reapProcess(reclaimPid));
    assert(reclaimKernel.memoryManager().freeFrames()==freeBefore);
    assert(reclaimKernel.processes().empty());
    nexa::NexaKernel quotaKernel(1024);
    const auto quotaPid=quotaKernel.spawn("memory-quota",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,1},{nexa::Opcode::Store,300},
        {nexa::Opcode::LoadImmediate,2},{nexa::Opcode::Store,600},
        {nexa::Opcode::Halt,0}}});
    quotaKernel.writeProcessMemory(quotaPid,300,7); // establish one data page before lowering the limit
    quotaKernel.setMemoryPageLimit(quotaPid,2); // one code page plus one data page
    bool belowUsageRejected=false;
    try { quotaKernel.setMemoryPageLimit(quotaPid,1); }
    catch(const std::invalid_argument&) { belowUsageRejected=true; }
    assert(belowUsageRejected);
    quotaKernel.run();
    bool quotaFaulted=false;
    for(const auto& process:quotaKernel.processes()) if(process.pid==quotaPid) {
        quotaFaulted=process.state==nexa::ProcessState::Faulted&&
            process.faultReason.find("memory page limit exceeded")!=std::string::npos&&
            process.memoryPageLimit==2&&process.virtualPages==2;
    }
    assert(quotaFaulted);
    quotaKernel.setMemoryPageLimit(quotaPid,0);
    for(const auto& process:quotaKernel.processes()) if(process.pid==quotaPid) assert(process.memoryPageLimit==0);

    nexa::NexaKernel zeroQuotaKernel(512);
    const auto zeroQuotaPid=zeroQuotaKernel.spawn("zero-data-quota",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,9},{nexa::Opcode::Store,300},{nexa::Opcode::Halt,0}}});
    zeroQuotaKernel.setMemoryPageLimit(zeroQuotaPid,1); // executable page only; no data pages allowed
    zeroQuotaKernel.run();
    bool zeroQuotaFaulted=false;
    for(const auto& process:zeroQuotaKernel.processes()) if(process.pid==zeroQuotaPid)
        zeroQuotaFaulted=process.state==nexa::ProcessState::Faulted&&
            process.faultReason.find("memory page limit exceeded")!=std::string::npos&&process.virtualPages==1;
    assert(zeroQuotaFaulted);

    nexa::NexaKernel kernel(1024);
    nexa::Executable basic{0,{{nexa::Opcode::LoadImmediate,65},{nexa::Opcode::Output,nexa::VM::DisplayPort},{nexa::Opcode::Halt,0}}};
    const auto first=kernel.spawn("alpha",basic);
    const auto second=kernel.spawn("beta",basic);
    assert(first!=second);
    kernel.filesystem().createFile("/7",{7});
    assert(!kernel.hasFileAccess(first,"/7",nexa::FilePermission::Read));
    kernel.grantFileAccess(first,"/7",nexa::FilePermission::Read);
    assert(kernel.hasFileAccess(first,"/7",nexa::FilePermission::Read));
    assert(!kernel.hasFileAccess(first,"/7",nexa::FilePermission::Write));
    assert(!kernel.hasFileAccess(second,"/7",nexa::FilePermission::Read));
    kernel.writeProcessMemory(first,100,'/');
    kernel.writeProcessMemory(first,101,'7');
    kernel.writeProcessMemory(first,102,0);
    const auto readHandle=kernel.syscall(first,nexa::Syscall::OpenRead,100);
    assert(readHandle!=0);
    assert(kernel.syscall(first,nexa::Syscall::ReadByte,readHandle)==7);
    assert(kernel.syscall(first,nexa::Syscall::ReadByte,readHandle)==0);
    assert(kernel.syscall(first,nexa::Syscall::CloseFile,readHandle)==0);
    kernel.grantFileAccess(first,"/7",nexa::FilePermission::Write);
    const auto writeHandle=kernel.syscall(first,nexa::Syscall::OpenWrite,100);
    kernel.writeProcessMemory(first,110,writeHandle);
    kernel.writeProcessMemory(first,111,88);
    assert(kernel.syscall(first,nexa::Syscall::WriteByte,110)==0);
    assert(kernel.filesystem().readFile("/7")==std::vector<std::uint8_t>{88});
    assert(kernel.syscall(first,nexa::Syscall::CloseFile,writeHandle)==0);
    bool badPermissionPath=false;
    try { kernel.grantFileAccess(first,"/../7",nexa::FilePermission::Read); }
    catch(const std::invalid_argument&) { badPermissionPath=true; }
    assert(badPermissionPath);
    kernel.writeProcessMemory(first,20,42);
    assert(kernel.readProcessMemory(first,20)==42);
    assert(kernel.readProcessMemory(second,20)==0);
    bool bounds=false;
    try { kernel.writeProcessMemory(first,1024,1); } catch(const std::out_of_range&) { bounds=true; }
    assert(bounds);
    assert(kernel.scheduleOne()); assert(kernel.scheduleOne()); kernel.run();
    auto processes=kernel.processes();
    assert(processes.size()==2);
    assert(processes[0].state==nexa::ProcessState::Halted);
    assert(processes[1].state==nexa::ProcessState::Halted);
    assert(kernel.takeConsoleOutput()=="AA");
    assert(kernel.syscall(first,nexa::Syscall::GetPid)==(first&255));
    assert(kernel.syscall(first,nexa::Syscall::Write,'!')==0);
    assert(kernel.takeConsoleOutput()=="!");
    nexa::Executable guest{0,{{nexa::Opcode::LoadImmediate,1},{nexa::Opcode::LoadBImmediate,'H'},{nexa::Opcode::Syscall,0},{nexa::Opcode::LoadImmediate,0},{nexa::Opcode::Syscall,0}}};
    const auto sharedGuestPid=kernel.spawn("guest-syscall",guest);
    kernel.run();
    assert(kernel.takeConsoleOutput()=="H");
    processes=kernel.processes();
    bool guestHalted=false,guestExitCode=false;
    for(const auto& process:processes) if(process.pid==sharedGuestPid) {
        guestHalted=process.state==nexa::ProcessState::Halted;
        guestExitCode=process.exitCode&&*process.exitCode=='H'&&process.terminationReason=="exit";
    }
    assert(guestHalted&&guestExitCode);
    nexa::Executable invalid{0,{{nexa::Opcode::LoadImmediate,99},{nexa::Opcode::Syscall,0}}};
    const auto badPid=kernel.spawn("invalid-syscall",invalid);
    // A faulting guest is isolated; the scheduler continues and records its fault.
    const auto healthyPid=kernel.spawn("still-healthy",basic);
    kernel.run();
    processes=kernel.processes();
    bool badFaulted=false,healthyHalted=false;
    for(const auto& process:processes) {
        if(process.pid==badPid) badFaulted=process.state==nexa::ProcessState::Faulted&&!process.faultReason.empty()&&process.terminationReason=="fault";
        if(process.pid==healthyPid) healthyHalted=process.state==nexa::ProcessState::Halted&&process.terminationReason=="halt";
    }
    assert(badFaulted&&healthyHalted);
    assert(kernel.takeConsoleOutput()=="A");
    nexa::NexaShell shell(kernel);
    assert(shell.execute("memlimit "+std::to_string(first)+" 2")=="memory page limit updated");
    assert(shell.execute("ps").find("memlimit=2")!=std::string::npos);
    assert(shell.execute("memlimit "+std::to_string(first)+" 1").find("error: Memory page limit") == 0);
    const auto oomSnapshot=shell.execute("oomstat");
    assert(oomSnapshot.find("pressure=normal")!=std::string::npos);
    assert(oomSnapshot.find("frames_free=")!=std::string::npos);
    assert(oomSnapshot.find("swap_outs=")!=std::string::npos);
    assert(oomSnapshot.find("recovery_attempts=")!=std::string::npos);
    assert(shell.execute("pwd")=="/");
    assert(shell.execute("whoami")=="root");
    assert(shell.execute("ps").find("fault=")!=std::string::npos);
    assert(shell.execute("passwd root-secret")=="password updated");
    assert(shell.execute("useradd guest guest-secret")=="created user guest");
    assert(shell.execute("login guest wrong-secret")=="authentication failed");
    assert(shell.execute("write /shared.txt hello")=="wrote 5 bytes");
    assert(shell.execute("login guest guest-secret")=="logged in as guest");
    assert(shell.execute("cat /shared.txt")=="hello");
    assert(shell.execute("write /shared.txt blocked")=="permission denied");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("chmod 600 /shared.txt")=="mode updated");
    assert(shell.execute("login guest guest-secret")=="logged in as guest");
    assert(shell.execute("cat /shared.txt")=="permission denied");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("chown guest /shared.txt")=="owner updated");
    assert(shell.execute("login guest guest-secret")=="logged in as guest");
    assert(shell.execute("write /shared.txt allowed")=="wrote 7 bytes");
    assert(shell.execute("login root root-secret")=="logged in as root");
    for(int attempt=0;attempt<4;++attempt) assert(shell.execute("login guest bad-password")=="authentication failed");
    assert(shell.execute("login guest bad-password")=="account locked; contact root");
    assert(shell.execute("login guest guest-secret")=="account locked; contact root");
    assert(shell.execute("passwd guest renewed-secret")=="password reset");
    assert(shell.execute("login guest renewed-secret")=="logged in as guest");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("groupadd developers")=="created group developers");
    assert(shell.execute("groupadd qa")=="created group qa");
    assert(shell.execute("usermod -aG qa guest")=="supplementary group added");
    assert(shell.execute("groups guest")=="guest qa");
    assert(shell.execute("usermod -aG qa guest")=="unknown user/group or membership exists");
    assert(shell.execute("usermod -rG qa guest")=="supplementary group removed");
    assert(shell.execute("groups guest")=="guest");
    assert(shell.execute("usermod -aG qa guest")=="supplementary group added");
    assert(shell.execute("usermod -g developers guest")=="primary group updated");
    assert(shell.execute("groups guest")=="developers qa");
    assert(shell.execute("write /team.txt team-data")=="wrote 9 bytes");
    assert(shell.execute("chgrp developers /team.txt")=="group updated");
    assert(shell.execute("chmod 640 /team.txt")=="mode updated");
    assert(shell.execute("login guest renewed-secret")=="logged in as guest");
    assert(shell.execute("cat /team.txt")=="team-data");
    assert(shell.execute("write /team.txt denied")=="permission denied");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("chmod 660 /team.txt")=="mode updated");
    assert(shell.execute("login guest renewed-secret")=="logged in as guest");
    assert(shell.execute("write /team.txt shared")=="wrote 6 bytes");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("savefs /tmp/nexa_security_test.img")=="filesystem image saved");
    nexa::NexaKernel restoredKernel(1024);
    nexa::NexaShell restoredShell(restoredKernel);
    assert(restoredShell.execute("loadfs /tmp/nexa_security_test.img")=="filesystem image loaded");
    assert(restoredShell.execute("whoami")=="root");
    assert(restoredShell.execute("login guest renewed-secret")=="logged in as guest");
    assert(restoredShell.execute("cat /shared.txt")=="allowed");
    assert(restoredShell.execute("write /shared.txt persisted")=="wrote 9 bytes");
    std::remove("/tmp/nexa_security_test.img");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("mkdir /apps")=="created directory /apps");
    assert(shell.execute("cd /apps")=="/apps");
    assert(shell.execute("pwd")=="/apps");
    assert(shell.execute("write hello.nexa HALT")=="wrote 4 bytes");
    assert(shell.execute("cat hello.nexa").find("HALT")==0);
    assert(shell.execute("ls").find("hello.nexa")!=std::string::npos);
    assert(shell.execute("run hello.nexa")=="");
    assert(shell.execute("ps").find("hello.nexa halted")!=std::string::npos);
    (void)shell.execute("oomreap");
    assert(shell.execute("ps").find("hello.nexa")==std::string::npos);
    assert(shell.execute("rm hello.nexa")=="removed /apps/hello.nexa");
    assert(shell.execute("cd ..")=="/");
    assert(shell.execute("chmod 700 /apps")=="mode updated");
    assert(shell.execute("login guest renewed-secret")=="logged in as guest");
    assert(shell.execute("ls /apps")=="permission denied");
    assert(shell.execute("cd /apps")=="permission denied");
    assert(shell.execute("touch /apps/blocked.txt")=="permission denied");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("logout")=="session closed");
    assert(shell.execute("whoami")=="(not logged in)");
    assert(shell.execute("pwd")=="session locked; login first");
    assert(shell.execute("ls")=="session locked; login first");
    assert(shell.execute("login guest renewed-secret")=="logged in as guest");
    assert(shell.execute("whoami")=="guest");
    assert(shell.execute("pwd")=="/");
}
