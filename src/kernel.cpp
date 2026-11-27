#include "nexa/kernel.hpp"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <limits>
#include <sstream>
namespace nexa {
NexaKernel::NexaKernel(std::size_t n):ramBytes_(n),memory_(std::max<std::size_t>(VirtualMemoryManager::PageSize,((n+VirtualMemoryManager::PageSize-1)/VirtualMemoryManager::PageSize)*VirtualMemoryManager::PageSize*16)){
    VM check(n); (void)check;
    memory_.setPageFaultHandler([this](std::uint32_t pid,std::uint8_t page){
        if((pid&0x80000000u)!=0) throw std::runtime_error("Execute address space page fault");
        memory_.mapPage(pid,page,true,true,false);
    });
}
std::uint32_t NexaKernel::spawn(const std::string& name,const Executable& image) {
    if(name.empty()) throw std::invalid_argument("Process name cannot be empty");
    if(image.code.empty()||image.entryPoint>=image.code.size()||image.code.size()>65536/3) throw std::invalid_argument("Invalid process executable");
    const auto pid=nextPid_++;
    Process p{ProcessInfo{pid,name,ProcessState::Ready,0,{}},VM(ramBytes_)};
    p.machine.loadProgram(image.code);
    p.machine.setProgramCounter(image.entryPoint);
    p.machine.setSyscallHandler([this,pid](std::uint8_t service,std::uint8_t argument) {
        if(service>static_cast<std::uint8_t>(Syscall::SharedCompareSwap16)) throw std::runtime_error("Unknown kernel syscall");
        return syscall(pid,static_cast<Syscall>(service),argument);
    });
    const auto codeSpace=pid|0x80000000u;
    memory_.createAddressSpace(pid);
    bool codeSpaceCreated=false;
    try {
        memory_.createAddressSpace(codeSpace);
        codeSpaceCreated=true;
        const auto codePages=((image.code.size()*3)+VirtualMemoryManager::PageSize-1)/VirtualMemoryManager::PageSize;
        for(std::size_t page=0;page<codePages;++page)
            memory_.mapPage(codeSpace,static_cast<std::uint8_t>(page),true,true,true);
        for(std::size_t index=0;index<image.code.size();++index) {
            const auto address=index*3;
            const auto& instruction=image.code[index];
            memory_.write(codeSpace,static_cast<std::uint16_t>(address),static_cast<std::uint8_t>(instruction.opcode),MemoryAccess::Kernel);
            memory_.write(codeSpace,static_cast<std::uint16_t>(address+1),static_cast<std::uint8_t>(instruction.operand&0xFF),MemoryAccess::Kernel);
            memory_.write(codeSpace,static_cast<std::uint16_t>(address+2),static_cast<std::uint8_t>(instruction.operand>>8),MemoryAccess::Kernel);
        }
        for(std::size_t page=0;page<codePages;++page)
            memory_.protectPage(codeSpace,static_cast<std::uint8_t>(page),false,true,true);
    } catch(...) {
        if(codeSpaceCreated) memory_.destroyAddressSpace(codeSpace);
        memory_.destroyAddressSpace(pid);
        throw;
    }
    p.machine.setMemoryAccessors(
        [this,pid](std::uint16_t address){return memory_.read(pid,address,MemoryAccess::User);},
        [this,pid](std::uint16_t address,std::uint8_t value){memory_.write(pid,address,value,MemoryAccess::User);});
    p.machine.setInstructionFetchGuard([this,codeSpace](std::uint16_t instructionIndex){
        const auto byteAddress=static_cast<std::size_t>(instructionIndex)*3;
        if(byteAddress>65533) throw std::runtime_error("Instruction address exceeds code memory");
        (void)memory_.canExecute(codeSpace,static_cast<std::uint16_t>(byteAddress),MemoryAccess::User);
    });
    p.machine.setInstructionReader([this,codeSpace](std::uint16_t instructionIndex){
        const auto address=static_cast<std::size_t>(instructionIndex)*3;
        if(address>65533) throw std::runtime_error("Instruction address exceeds code memory");
        std::uint8_t bytes[3]{};
        for(std::size_t offset=0;offset<3;++offset) {
            const auto virtualAddress=static_cast<std::uint16_t>(address+offset);
            (void)memory_.canExecute(codeSpace,virtualAddress,MemoryAccess::User);
            bytes[offset]=memory_.read(codeSpace,virtualAddress,MemoryAccess::Kernel);
        }
        if(bytes[0]>static_cast<std::uint8_t>(Opcode::Halt)) throw std::runtime_error("Invalid instruction opcode in guest memory");
        const auto operand=static_cast<std::uint16_t>(bytes[1]|(static_cast<std::uint16_t>(bytes[2])<<8));
        return Instruction{static_cast<Opcode>(bytes[0]),operand};
    });
    processes_.emplace(pid,std::move(p));
    filePermissions_[pid] = {};
    return pid;
}
bool NexaKernel::hasDeadlock() const noexcept {
    bool blocked=false;
    for(const auto& [pid,proc]:processes_) {
        (void)pid;
        if(proc.info.state==ProcessState::Ready||proc.info.state==ProcessState::Running) return false;
        if(proc.info.state==ProcessState::Blocked) blocked=true;
    }
    return blocked;
}
std::vector<std::uint32_t> NexaKernel::mutexDeadlockCycle() const {
    std::map<std::uint32_t,std::uint8_t> visitState;
    std::map<std::uint32_t,std::size_t> activePosition;
    std::vector<std::uint32_t> path,cycle;
    const auto visit=[&](const auto& self,std::uint32_t pid)->bool {
        auto& state=visitState[pid];
        if(state==1) {
            const auto begin=activePosition.find(pid);
            if(begin!=activePosition.end()) cycle.assign(path.begin()+static_cast<std::ptrdiff_t>(begin->second),path.end());
            return true;
        }
        if(state==2) return false;
        state=1;
        activePosition[pid]=path.size();
        path.push_back(pid);
        for(const auto& [id,mutex]:mutexes_) {
            (void)id;
            if(!mutex.owner||std::find(mutex.waiters.begin(),mutex.waiters.end(),pid)==mutex.waiters.end()) continue;
            const auto owner=processes_.find(*mutex.owner);
            if(owner!=processes_.end()&&owner->second.info.state!=ProcessState::Halted&&
               owner->second.info.state!=ProcessState::Faulted&&self(self,*mutex.owner)) return true;
        }
        path.pop_back();
        activePosition.erase(pid);
        state=2;
        return false;
    };
    for(const auto& [pid,process]:processes_) {
        if(process.info.state==ProcessState::Blocked&&visit(visit,pid)) break;
    }
    return cycle;
}
bool NexaKernel::hasMutexDeadlock() const {
    return !mutexDeadlockCycle().empty();
}

bool NexaKernel::terminateProcess(std::uint32_t pid) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) return false;
    if(it->second.info.state==ProcessState::Halted||it->second.info.state==ProcessState::Faulted) return false;
    it->second.machine.requestHalt();
    it->second.info.state=ProcessState::Halted;
    it->second.info.exitCode=137;
    it->second.info.terminationReason="killed";
    it->second.openFiles.clear();
    releaseSharedAttachments(pid);
    it->second.inbox.clear();
    it->second.signals.clear();
    cleanupSynchronizationState(pid);
    releaseOwnedMutexes(pid);
    if(currentPid_==pid) currentPid_=0;
    return true;
}
bool NexaKernel::sendSignal(std::uint32_t pid,ProcessSignal signal) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    auto& process=it->second;
    if(process.info.state==ProcessState::Halted||process.info.state==ProcessState::Faulted) return false;
    switch(signal) {
    case ProcessSignal::Terminate:
    case ProcessSignal::Kill:
        process.machine.requestHalt();
        process.info.state=ProcessState::Halted;
        process.info.exitCode=signal==ProcessSignal::Terminate?143:137;
        process.info.terminationReason=signal==ProcessSignal::Terminate?"signal:TERM":"signal:KILL";
        process.openFiles.clear();
        releaseSharedAttachments(pid);
        process.inbox.clear();
        process.signals.clear();
        cleanupSynchronizationState(pid);
        releaseOwnedMutexes(pid);
        if(currentPid_==pid) currentPid_=0;
        return true;
    case ProcessSignal::Stop:
        if(process.info.state==ProcessState::Stopped) return false;
        process.info.state=ProcessState::Stopped;
        if(currentPid_==pid) currentPid_=0;
        return true;
    case ProcessSignal::Continue:
        if(process.info.state!=ProcessState::Stopped) return false;
        process.info.state=ProcessState::Ready;
        return true;
    case ProcessSignal::User1:
    case ProcessSignal::User2:
        if(process.signals.size()>=MaxPendingSignals) return false;
        process.signals.push_back(signal);
        return true;
    }
    throw std::invalid_argument("Unsupported process signal");
}
std::optional<ProcessSignal> NexaKernel::receiveSignal(std::uint32_t pid) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    if(it->second.signals.empty()) return std::nullopt;
    const auto signal=it->second.signals.front();
    it->second.signals.pop_front();
    return signal;
}
std::uint8_t NexaKernel::createSemaphore(std::uint8_t initial,std::uint8_t maximum) {
    if(maximum==0||initial>maximum) throw std::invalid_argument("Semaphore requires 0 <= initial <= maximum and maximum > 0");
    if(nextSemaphore_>MaxSyncObjects) throw std::runtime_error("Semaphore table exhausted");
    const auto id=static_cast<std::uint8_t>(nextSemaphore_++);
    semaphores_.emplace(id,Semaphore{initial,maximum,{}});
    return id;
}
bool NexaKernel::semaphoreWait(std::uint32_t pid,std::uint8_t id) {
    auto p=processes_.find(pid);auto s=semaphores_.find(id);
    if(p==processes_.end()) throw std::out_of_range("Unknown process id");
    if(s==semaphores_.end()) throw std::out_of_range("Unknown semaphore id");
    if(p->second.info.state==ProcessState::Halted||p->second.info.state==ProcessState::Faulted) return false;
    auto& sem=s->second;
    auto grant=std::find(p->second.semaphoreGrants.begin(),p->second.semaphoreGrants.end(),id);
    if(grant!=p->second.semaphoreGrants.end()){p->second.semaphoreGrants.erase(grant);return true;}
    if(sem.count>0){--sem.count;return true;}
    if(std::find(sem.waiters.begin(),sem.waiters.end(),pid)==sem.waiters.end()) sem.waiters.push_back(pid);
    p->second.info.state=ProcessState::Blocked;
    p->second.machine.retryCurrentInstruction();
    return false;
}
bool NexaKernel::semaphoreSignal(std::uint8_t id) {
    auto s=semaphores_.find(id);if(s==semaphores_.end())throw std::out_of_range("Unknown semaphore id");
    auto& sem=s->second;
    while(!sem.waiters.empty()){
        const auto pid=sem.waiters.front();sem.waiters.pop_front();
        auto p=processes_.find(pid);
        if(p!=processes_.end()&&p->second.info.state==ProcessState::Blocked){p->second.semaphoreGrants.push_back(id);p->second.info.state=ProcessState::Ready;return true;}
    }
    if(sem.count==sem.maximum)return false;
    ++sem.count;return true;
}
std::uint8_t NexaKernel::createMutex() {
    if(nextMutex_>MaxSyncObjects)throw std::runtime_error("Mutex table exhausted");
    const auto id=static_cast<std::uint8_t>(nextMutex_++);
    mutexes_.emplace(id,Mutex{});
    return id;
}
bool NexaKernel::mutexLock(std::uint32_t pid,std::uint8_t id) {
    auto p=processes_.find(pid);auto m=mutexes_.find(id);
    if(p==processes_.end())throw std::out_of_range("Unknown process id");
    if(m==mutexes_.end())throw std::out_of_range("Unknown mutex id");
    if(p->second.info.state==ProcessState::Halted||p->second.info.state==ProcessState::Faulted)return false;
    auto& mutex=m->second;
    auto grant=std::find(p->second.mutexGrants.begin(),p->second.mutexGrants.end(),id);
    if(grant!=p->second.mutexGrants.end()){p->second.mutexGrants.erase(grant);return true;}
    if(!mutex.owner){mutex.owner=pid;return true;}
    if(*mutex.owner==pid)return false;
    if(std::find(mutex.waiters.begin(),mutex.waiters.end(),pid)==mutex.waiters.end())mutex.waiters.push_back(pid);
    p->second.info.state=ProcessState::Blocked;
    p->second.machine.retryCurrentInstruction();
    return false;
}
bool NexaKernel::mutexUnlock(std::uint32_t pid,std::uint8_t id) {
    auto m=mutexes_.find(id);if(m==mutexes_.end())throw std::out_of_range("Unknown mutex id");
    auto& mutex=m->second;
    if(!mutex.owner||*mutex.owner!=pid)return false;
    while(!mutex.waiters.empty()){
        const auto next=mutex.waiters.front();mutex.waiters.pop_front();
        auto p=processes_.find(next);
        if(p!=processes_.end()&&p->second.info.state==ProcessState::Blocked){mutex.owner=next;p->second.mutexGrants.push_back(id);p->second.info.state=ProcessState::Ready;return true;}
    }
    mutex.owner.reset();return true;
}
void NexaKernel::releaseSharedAttachments(std::uint32_t pid) {
    auto p=processes_.find(pid);if(p==processes_.end())return;
    for(auto id:p->second.sharedAttachments){unmapSharedMemory(pid,id);auto s=sharedSegments_.find(id);if(s!=sharedSegments_.end())s->second.attached.erase(pid);}
    p->second.sharedAttachments.clear();
}
void NexaKernel::cleanupSynchronizationState(std::uint32_t pid) {
    for(auto& [id,sem]:semaphores_) {
        sem.waiters.erase(std::remove(sem.waiters.begin(),sem.waiters.end(),pid),sem.waiters.end());
        auto process=processes_.find(pid);
        if(process==processes_.end()) continue;
        auto grant=std::find(process->second.semaphoreGrants.begin(),process->second.semaphoreGrants.end(),id);
        if(grant==process->second.semaphoreGrants.end()) continue;
        process->second.semaphoreGrants.erase(grant);
        bool handedOff=false;
        while(!sem.waiters.empty()) {
            const auto next=sem.waiters.front();sem.waiters.pop_front();
            auto waiter=processes_.find(next);
            if(waiter==processes_.end()||waiter->second.info.state!=ProcessState::Blocked) continue;
            waiter->second.semaphoreGrants.push_back(id);
            waiter->second.info.state=ProcessState::Ready;
            handedOff=true;
            break;
        }
        if(!handedOff&&sem.count<sem.maximum) ++sem.count;
    }
    for(auto& [id,mutex]:mutexes_)
        mutex.waiters.erase(std::remove(mutex.waiters.begin(),mutex.waiters.end(),pid),mutex.waiters.end());
    if(auto process=processes_.find(pid);process!=processes_.end()) {
        process->second.semaphoreGrants.clear();
        process->second.mutexGrants.clear();
    }
}
void NexaKernel::releaseOwnedMutexes(std::uint32_t pid) {
    for(auto& [id,mutex]:mutexes_) {
        if(!mutex.owner||*mutex.owner!=pid) continue;
        mutex.owner.reset();
        while(!mutex.waiters.empty()) {
            const auto next=mutex.waiters.front();mutex.waiters.pop_front();
            auto waiter=processes_.find(next);
            if(waiter==processes_.end()||waiter->second.info.state!=ProcessState::Blocked) continue;
            mutex.owner=next;
            waiter->second.mutexGrants.push_back(id);
            waiter->second.info.state=ProcessState::Ready;
            break;
        }
    }
}
bool NexaKernel::reapProcess(std::uint32_t pid) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) return false;
    if(it->second.info.state!=ProcessState::Halted&&it->second.info.state!=ProcessState::Faulted) return false;
    memory_.destroyAddressSpace(pid);
    const auto codeSpace=pid|0x80000000u;
    memory_.destroyAddressSpace(codeSpace);
    filePermissions_.erase(pid);
    processes_.erase(it);
    if(currentPid_==pid) currentPid_=0;
    return true;
}
void NexaKernel::setPriority(std::uint32_t pid,std::uint8_t priority) {
    if(priority>7) throw std::invalid_argument("Process priority must be between 0 and 7");
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    it->second.info.priority=priority;
    it->second.readyWaitTicks=0;
}
void NexaKernel::setInstructionBudget(std::uint32_t pid,std::uint64_t instructions) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    if(it->second.info.state==ProcessState::Halted||it->second.info.state==ProcessState::Faulted)
        throw std::logic_error("Cannot change instruction budget of a stopped process");
    it->second.info.instructionBudget=instructions;
    if(instructions!=0&&it->second.info.ticks>=instructions) {
        it->second.machine.requestHalt();
        it->second.info.state=ProcessState::Halted;
        it->second.info.exitCode=124;
        it->second.info.terminationReason="budget";
        it->second.openFiles.clear();
        it->second.inbox.clear();
        it->second.signals.clear();
        cleanupSynchronizationState(pid);
        releaseOwnedMutexes(pid);
    }
}
void NexaKernel::setTimeQuantum(std::size_t instructions) {
    if(instructions==0||instructions>1000000) throw std::invalid_argument("Time quantum must be between 1 and 1000000 instructions");
    timeQuantum_=instructions;
}
void NexaKernel::setPriorityAgingThreshold(std::uint64_t ticks) {
    priorityAgingThreshold_=ticks;
    for(auto& [id,proc]:processes_) { (void)id; proc.readyWaitTicks=0; }
}
void NexaKernel::setTimerInterval(std::uint64_t ticks) {
    timerInterval_=ticks;
}
bool NexaKernel::acknowledgeTimerInterrupt() noexcept {
    if(pendingTimerInterrupts_==0) return false;
    --pendingTimerInterrupts_;
    return true;
}
bool NexaKernel::sendMessage(std::uint32_t from,std::uint32_t to,std::vector<std::uint8_t> payload) {
    auto sender=processes_.find(from),receiver=processes_.find(to);
    if(sender==processes_.end()||receiver==processes_.end()) throw std::out_of_range("Unknown IPC process id");
    if(sender->second.info.state==ProcessState::Halted||sender->second.info.state==ProcessState::Faulted) return false;
    if(receiver->second.info.state==ProcessState::Halted||receiver->second.info.state==ProcessState::Faulted) return false;
    if(payload.size()>MaxIpcPayload||receiver->second.inbox.size()>=MaxInboxMessages) return false;
    receiver->second.inbox.push_back(IpcMessage{from,std::move(payload)});
    if(receiver->second.info.state==ProcessState::Blocked) receiver->second.info.state=ProcessState::Ready;
    return true;
}
std::optional<IpcMessage> NexaKernel::receiveMessage(std::uint32_t pid) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown IPC process id");
    if(it->second.inbox.empty()) return std::nullopt;
    auto message=std::move(it->second.inbox.front());
    it->second.inbox.pop_front();
    return message;
}
bool NexaKernel::scheduleOne() {
    if(processes_.empty()) return false;
    std::vector<std::uint32_t> ids;
    std::uint8_t highestPriority=0;
    bool haveReady=false;
    const auto effectivePriority=[this](const Process& candidate) { return this->effectivePriority(candidate.info.pid); };
    for(const auto& [id,proc]:processes_) {
        if(proc.info.state!=ProcessState::Ready) continue;
        const auto effective=effectivePriority(proc);
        if(!haveReady||effective>highestPriority) {
            ids.clear(); highestPriority=effective; haveReady=true;
        }
        if(effective==highestPriority) ids.push_back(id);
    }
    if(ids.empty()) return false;
    auto it=std::find_if(ids.begin(),ids.end(),[&](auto id){return id>cursor_;});
    const auto pid=it==ids.end()?ids.front():*it;
    cursor_=pid; currentPid_=pid;
    auto& proc=processes_.at(pid);
    proc.info.state=ProcessState::Running;
    proc.readyWaitTicks=0;
    proc.yieldRequested=false;
    for(std::size_t instruction=0;instruction<timeQuantum_;++instruction) {
        bool timerFired=false;
        try {
            proc.machine.stepInstruction();
            ++proc.info.ticks;
            ++systemTicks_;
            if(proc.info.instructionBudget!=0&&proc.info.ticks>=proc.info.instructionBudget) {
                proc.machine.requestHalt();
                proc.info.state=ProcessState::Halted;
                proc.info.exitCode=124;
                proc.info.terminationReason="budget";
                proc.openFiles.clear();
                releaseSharedAttachments(pid);
                proc.inbox.clear();
                break;
            }
            for(auto& [waitingPid,waiting]:processes_) {
                if(waitingPid!=pid&&waiting.info.state==ProcessState::Ready&&waiting.readyWaitTicks<std::numeric_limits<std::uint64_t>::max())
                    ++waiting.readyWaitTicks;
            }
            if(timerInterval_!=0&&systemTicks_%timerInterval_==0) {
                ++timerInterrupts_;
                ++pendingTimerInterrupts_;
                timerFired=true;
            }
            output_+=proc.machine.deviceOutput();
            proc.machine.clearDeviceOutput();
            if(proc.machine.halted()||proc.yieldRequested||proc.info.state==ProcessState::Blocked) break;
            if(timerFired) break;
        } catch(const std::exception& e) {
            proc.info.state=ProcessState::Faulted;
            proc.info.terminationReason="fault";
            proc.info.faultReason=e.what();
            output_+=proc.machine.deviceOutput();
            proc.machine.clearDeviceOutput();
            break;
        } catch(...) {
            proc.info.state=ProcessState::Faulted;
            proc.info.terminationReason="fault";
            proc.info.faultReason="Unknown process fault";
            break;
        }
    }
    if(proc.info.state!=ProcessState::Faulted&&proc.info.state!=ProcessState::Blocked) {
        proc.info.state=proc.machine.halted()?ProcessState::Halted:ProcessState::Ready;
        if(proc.info.state==ProcessState::Halted&&proc.info.terminationReason.empty())
            proc.info.terminationReason="halt";
    }
    if(proc.info.state==ProcessState::Faulted||proc.info.state==ProcessState::Halted) {
        proc.openFiles.clear();
        releaseSharedAttachments(pid);
        proc.inbox.clear();
        proc.signals.clear();
        cleanupSynchronizationState(pid);
        releaseOwnedMutexes(pid);
    }
    proc.yieldRequested=false;
    return true;
}
void NexaKernel::run(std::size_t maxSlices) { for(std::size_t n=0;n<maxSlices&&scheduleOne();++n) {} }
std::uint8_t NexaKernel::effectivePriority(std::uint32_t pid) const {
    auto target=processes_.find(pid);
    if(target==processes_.end()) throw std::out_of_range("Unknown process id");
    std::map<std::uint32_t,std::uint8_t> priorities;
    for(const auto& [id,proc]:processes_) {
        const auto aging=priorityAgingThreshold_==0?0:proc.readyWaitTicks/priorityAgingThreshold_;
        priorities[id]=static_cast<std::uint8_t>(std::min<std::uint64_t>(7,proc.info.priority+aging));
    }
    for(std::size_t pass=0;pass<processes_.size();++pass) {
        bool changed=false;
        for(const auto& [mid,mutex]:mutexes_) {
            (void)mid;
            if(!mutex.owner||!priorities.contains(*mutex.owner)) continue;
            auto& ownerPriority=priorities[*mutex.owner];
            for(const auto waiterId:mutex.waiters) {
                auto waiter=priorities.find(waiterId);
                if(waiter!=priorities.end()&&waiter->second>ownerPriority) {
                    ownerPriority=waiter->second;changed=true;
                }
            }
        }
        if(!changed) break;
    }
    return priorities.at(pid);
}
std::uint8_t NexaKernel::createSharedMemory(std::size_t bytes) {
    if(bytes==0||bytes>65536) throw std::invalid_argument("Shared memory size must be between 1 and 65536 bytes");
    if(nextSharedSegment_>MaxSyncObjects) throw std::runtime_error("Shared memory table exhausted");
    const auto id=static_cast<std::uint8_t>(nextSharedSegment_++);
    SharedSegment segment; segment.size=bytes; const auto pages=(bytes+VirtualMemoryManager::PageSize-1)/VirtualMemoryManager::PageSize; try { for(std::size_t i=0;i<pages;++i) segment.frames.push_back(memory_.createSharedFrame()); } catch(...) { for(auto frame:segment.frames) memory_.releaseSharedFrame(frame); throw; } sharedSegments_.emplace(id,std::move(segment));
    return id;
}
bool NexaKernel::destroySharedMemory(std::uint8_t id) {
    auto it=sharedSegments_.find(id);if(it==sharedSegments_.end())return false;
    for(auto pid:it->second.attached) {auto p=processes_.find(pid);if(p!=processes_.end()) {unmapSharedMemory(pid,id);p->second.sharedAttachments.erase(id);}}
    for(auto frame:it->second.frames) memory_.releaseSharedFrame(frame);
    sharedSegments_.erase(it);return true;
}
bool NexaKernel::attachSharedMemory(std::uint32_t pid,std::uint8_t id) {
    auto p=processes_.find(pid);auto s=sharedSegments_.find(id);
    if(p==processes_.end()||s==sharedSegments_.end())return false;
    if(p->second.info.state==ProcessState::Halted||p->second.info.state==ProcessState::Faulted)return false;
    p->second.sharedAttachments.insert(id);s->second.attached.insert(pid);return true;
}
bool NexaKernel::detachSharedMemory(std::uint32_t pid,std::uint8_t id) {
    auto p=processes_.find(pid);auto s=sharedSegments_.find(id);
    if(p==processes_.end()||s==sharedSegments_.end())return false;
    if(!p->second.sharedAttachments.erase(id))return false;
    s->second.attached.erase(pid);unmapSharedMemory(pid,id);return true;
}
std::optional<std::uint8_t> NexaKernel::readSharedMemory(std::uint32_t pid,std::uint8_t id,std::size_t offset) const {
    auto p=processes_.find(pid);auto s=sharedSegments_.find(id);
    if(p==processes_.end()||s==sharedSegments_.end()||!p->second.sharedAttachments.contains(id)||offset>=s->second.size)return std::nullopt;
    return memory_.readSharedFrame(s->second.frames[offset/VirtualMemoryManager::PageSize],offset%VirtualMemoryManager::PageSize);
}
bool NexaKernel::writeSharedMemory(std::uint32_t pid,std::uint8_t id,std::size_t offset,std::uint8_t value) {
    auto p=processes_.find(pid);auto s=sharedSegments_.find(id);
    if(p==processes_.end()||s==sharedSegments_.end()||!p->second.sharedAttachments.contains(id)||offset>=s->second.size)return false;
    auto mapping=p->second.sharedMappingWritable.find(id);if(mapping!=p->second.sharedMappingWritable.end()&&!mapping->second)return false;
    memory_.writeSharedFrame(s->second.frames[offset/VirtualMemoryManager::PageSize],offset%VirtualMemoryManager::PageSize,value);return true;
}
std::optional<std::uint8_t> NexaKernel::sharedAtomicAdd(std::uint32_t pid,std::uint8_t id,std::size_t offset,std::uint8_t delta) {
    const auto previous=readSharedMemory(pid,id,offset);
    if(!previous)return std::nullopt;
    if(!writeSharedMemory(pid,id,offset,static_cast<std::uint8_t>(*previous+delta)))return std::nullopt;
    return previous;
}
std::optional<std::uint8_t> NexaKernel::sharedCompareSwap(std::uint32_t pid,std::uint8_t id,std::size_t offset,std::uint8_t expected,std::uint8_t replacement) {
    const auto previous=readSharedMemory(pid,id,offset);
    if(!previous)return std::nullopt;
    if(*previous==expected&&!writeSharedMemory(pid,id,offset,replacement))return std::nullopt;
    return previous;
}
std::optional<std::uint16_t> NexaKernel::sharedAtomicAdd16(std::uint32_t pid,std::uint8_t id,std::size_t offset,std::uint16_t delta) {
    const auto low=readSharedMemory(pid,id,offset),high=readSharedMemory(pid,id,offset+1);
    if(!low||!high)return std::nullopt;
    const auto previous=static_cast<std::uint16_t>(*low|(static_cast<std::uint16_t>(*high)<<8));
    const auto next=static_cast<std::uint16_t>(previous+delta);
    if(!writeSharedMemory(pid,id,offset,static_cast<std::uint8_t>(next&0xff))||
       !writeSharedMemory(pid,id,offset+1,static_cast<std::uint8_t>(next>>8)))return std::nullopt;
    return previous;
}
std::optional<std::uint16_t> NexaKernel::sharedCompareSwap16(std::uint32_t pid,std::uint8_t id,std::size_t offset,std::uint16_t expected,std::uint16_t replacement) {
    const auto low=readSharedMemory(pid,id,offset),high=readSharedMemory(pid,id,offset+1);
    if(!low||!high)return std::nullopt;
    const auto previous=static_cast<std::uint16_t>(*low|(static_cast<std::uint16_t>(*high)<<8));
    if(previous==expected) {
        if(!writeSharedMemory(pid,id,offset,static_cast<std::uint8_t>(replacement&0xff))||
           !writeSharedMemory(pid,id,offset+1,static_cast<std::uint8_t>(replacement>>8)))return std::nullopt;
    }
    return previous;
}
bool NexaKernel::mapSharedMemory(std::uint32_t pid,std::uint8_t id,std::uint8_t basePage,bool writable) {
    auto p=processes_.find(pid);auto s=sharedSegments_.find(id);
    if(p==processes_.end()||s==sharedSegments_.end()||!p->second.sharedAttachments.contains(id))return false;
    const auto pages=s->second.frames.size();
    if(static_cast<std::size_t>(basePage)+pages>256)return false;
    if(p->second.sharedMappings.contains(id))return false;
    std::vector<std::uint8_t> mapped;
    try {
        for(std::size_t index=0;index<pages;++index) {
            const auto page=static_cast<std::uint8_t>(basePage+index);
            memory_.mapSharedFrame(pid,page,s->second.frames[index],writable,true);
            mapped.push_back(page);
        }
    } catch(...) { for(auto page:mapped) memory_.unmapPage(pid,page); return false; }
    p->second.sharedMappings[id]=basePage;
    p->second.sharedMappingWritable[id]=writable;
    return true;
}
bool NexaKernel::unmapSharedMemory(std::uint32_t pid,std::uint8_t id) {
    auto p=processes_.find(pid);if(p==processes_.end())return false;
    auto mapping=p->second.sharedMappings.find(id);if(mapping==p->second.sharedMappings.end())return false;
    const auto segment=sharedSegments_.find(id);
    const auto pages=segment==sharedSegments_.end()?0:segment->second.frames.size();
    for(std::size_t index=0;index<pages;++index) {
        const auto page=static_cast<std::uint8_t>(mapping->second+index);
        if(memory_.isMapped(pid,page))memory_.unmapPage(pid,page);
    }
    p->second.sharedMappings.erase(mapping);
    p->second.sharedMappingWritable.erase(id);
    return true;
}
std::vector<SharedMemoryInfo> NexaKernel::sharedMemoryInfo() const {
    std::vector<SharedMemoryInfo> result;result.reserve(sharedSegments_.size());
    for(const auto& [id,segment]:sharedSegments_)result.push_back({id,segment.size,segment.attached.size()});
    return result;
}
std::string NexaKernel::syncDiagnostics() const {
    std::ostringstream out;
    out<<"Semaphores: "<<semaphores_.size()<<"\\n";
    for(const auto& [id,sem]:semaphores_) out<<"  sem "<<static_cast<unsigned>(id)<<" count="<<sem.count<<"/"<<sem.maximum<<" waiters="<<sem.waiters.size()<<"\\n";
    out<<"Mutexes: "<<mutexes_.size()<<"\\n";
    for(const auto& [id,mutex]:mutexes_) {
        out<<"  mutex "<<static_cast<unsigned>(id)<<" owner=";
        if(mutex.owner) out<<*mutex.owner<<" (effective priority "<<static_cast<unsigned>(effectivePriority(*mutex.owner))<<")"; else out<<"none";
        out<<" waiters=[";
        bool first=true;for(auto waiter:mutex.waiters){if(!first)out<<",";first=false;out<<waiter<<"(p"<<static_cast<unsigned>(effectivePriority(waiter))<<")";}
        out<<"]\\n";
    }
    return out.str();
}
std::vector<ProcessInfo> NexaKernel::processes() const {
    std::vector<ProcessInfo> result;
    for(const auto& [id,p]:processes_) {
        (void)id;
        auto info=p.info;
        info.pendingMessages=p.inbox.size();
        info.pendingSignals=p.signals.size();
        info.readyWaitTicks=p.readyWaitTicks;
        info.effectivePriority=effectivePriority(id);
        result.push_back(std::move(info));
    }
    return result;
}
std::string NexaKernel::takeConsoleOutput() {
    // Drain any buffered device output as well as kernel-mediated writes.
    for (auto& [id, process] : processes_) {
        (void)id;
        output_ += process.machine.deviceOutput();
        process.machine.clearDeviceOutput();
    }
    auto result = std::move(output_);
    output_.clear();
    return result;
}
std::uint8_t NexaKernel::syscall(std::uint32_t pid,Syscall call,std::uint8_t arg) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    switch(call) {
    case Syscall::Exit: it->second.info.state=ProcessState::Halted; it->second.info.exitCode=arg; it->second.info.terminationReason="exit"; it->second.openFiles.clear(); releaseSharedAttachments(pid); it->second.inbox.clear(); it->second.signals.clear(); cleanupSynchronizationState(pid); releaseOwnedMutexes(pid); return 0;
    case Syscall::Write: output_.push_back(static_cast<char>(arg)); return 0;
    case Syscall::Read: return it->second.machine.readMemory(arg);
    case Syscall::GetPid: return static_cast<std::uint8_t>(pid&255);
    case Syscall::Yield: it->second.yieldRequested=true; return 0;
    case Syscall::GetSignal: { const auto signal=receiveSignal(pid); return signal?static_cast<std::uint8_t>(*signal):0; }
    case Syscall::SemaphoreWait: return semaphoreWait(pid,arg)?1:0;
    case Syscall::SemaphoreSignal: return semaphoreSignal(arg)?1:0;
    case Syscall::MutexLock: return mutexLock(pid,arg)?1:0;
    case Syscall::MutexUnlock: return mutexUnlock(pid,arg)?1:0;
    case Syscall::SharedCreate: {
        if(static_cast<std::size_t>(arg)+2>it->second.machine.ramSize()) throw std::runtime_error("Shared create descriptor outside process RAM");
        const auto low=it->second.machine.readMemory(arg);
        const auto high=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+1));
        const auto size=static_cast<std::size_t>(low|(static_cast<std::uint16_t>(high)<<8));
        return createSharedMemory(size);
    }
    case Syscall::SharedAttach: { if(static_cast<std::size_t>(arg)+2>it->second.machine.ramSize()) throw std::runtime_error("Shared attach descriptor outside process RAM"); const auto id=it->second.machine.readMemory(arg); const auto base=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+1)); if(!attachSharedMemory(pid,id))return 0; if(mapSharedMemory(pid,id,base))return 1; detachSharedMemory(pid,id); return 0; }
    case Syscall::SharedAttachReadOnly: { if(static_cast<std::size_t>(arg)+2>it->second.machine.ramSize()) throw std::runtime_error("Shared attach descriptor outside process RAM"); const auto id=it->second.machine.readMemory(arg); const auto base=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+1)); if(!attachSharedMemory(pid,id))return 0; if(mapSharedMemory(pid,id,base,false))return 1; detachSharedMemory(pid,id); return 0; }
    case Syscall::SharedDetach: { if(static_cast<std::size_t>(arg)+1>it->second.machine.ramSize()) throw std::runtime_error("Shared detach descriptor outside process RAM"); return detachSharedMemory(pid,it->second.machine.readMemory(arg))?1:0; }
    case Syscall::SharedRead: {
        if(static_cast<std::size_t>(arg)+2>it->second.machine.ramSize()) throw std::runtime_error("Shared-memory read descriptor outside process RAM");
        const auto segment=it->second.machine.readMemory(arg);
        const auto offset=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+1));
        const auto value=readSharedMemory(pid,segment,offset);
        if(!value) throw std::runtime_error("Shared-memory read denied or out of bounds");
        return *value;
    }
    case Syscall::SharedWrite: {
        if(static_cast<std::size_t>(arg)+3>it->second.machine.ramSize()) throw std::runtime_error("Shared-memory write descriptor outside process RAM");
        const auto segment=it->second.machine.readMemory(arg);
        const auto offset=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+1));
        const auto value=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+2));
        return writeSharedMemory(pid,segment,offset,value)?1:0;
    }
    case Syscall::SharedAtomicAdd: {
        if(static_cast<std::size_t>(arg)+3>it->second.machine.ramSize()) throw std::runtime_error("Shared atomic-add descriptor outside process RAM");
        const auto segment=it->second.machine.readMemory(arg);
        const auto offset=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+1));
        const auto delta=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+2));
        const auto previous=sharedAtomicAdd(pid,segment,offset,delta);
        if(!previous)throw std::runtime_error("Shared atomic-add denied or out of bounds");
        return *previous;
    }
    case Syscall::SharedAtomicAdd16: {
        if(static_cast<std::size_t>(arg)+5>it->second.machine.ramSize()) throw std::runtime_error("Shared 16-bit atomic-add descriptor outside process RAM");
        const auto id=it->second.machine.readMemory(arg),offset=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+1));
        const auto delta=static_cast<std::uint16_t>(it->second.machine.readMemory(static_cast<std::uint16_t>(arg+2))|(static_cast<std::uint16_t>(it->second.machine.readMemory(static_cast<std::uint16_t>(arg+3)))<<8));
        const auto previous=sharedAtomicAdd16(pid,id,offset,delta);
        if(!previous)throw std::runtime_error("Shared 16-bit atomic-add denied or out of bounds");
        it->second.machine.writeMemory(static_cast<std::uint16_t>(arg+4),static_cast<std::uint8_t>(*previous>>8));
        return static_cast<std::uint8_t>(*previous&0xff);
    }
    case Syscall::SharedCompareSwap16: {
        if(static_cast<std::size_t>(arg)+7>it->second.machine.ramSize()) throw std::runtime_error("Shared 16-bit compare-swap descriptor outside process RAM");
        const auto id=it->second.machine.readMemory(arg),offset=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+1));
        const auto expected=static_cast<std::uint16_t>(it->second.machine.readMemory(static_cast<std::uint16_t>(arg+2))|(static_cast<std::uint16_t>(it->second.machine.readMemory(static_cast<std::uint16_t>(arg+3)))<<8));
        const auto replacement=static_cast<std::uint16_t>(it->second.machine.readMemory(static_cast<std::uint16_t>(arg+4))|(static_cast<std::uint16_t>(it->second.machine.readMemory(static_cast<std::uint16_t>(arg+5)))<<8));
        const auto previous=sharedCompareSwap16(pid,id,offset,expected,replacement);
        if(!previous)throw std::runtime_error("Shared 16-bit compare-swap denied or out of bounds");
        it->second.machine.writeMemory(static_cast<std::uint16_t>(arg+6),static_cast<std::uint8_t>(*previous>>8));
        return static_cast<std::uint8_t>(*previous&0xff);
    }
    case Syscall::SharedCompareSwap: {
        if(static_cast<std::size_t>(arg)+4>it->second.machine.ramSize()) throw std::runtime_error("Shared compare-swap descriptor outside process RAM");
        const auto segment=it->second.machine.readMemory(arg);
        const auto offset=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+1));
        const auto expected=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+2));
        const auto replacement=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+3));
        const auto previous=sharedCompareSwap(pid,segment,offset,expected,replacement);
        if(!previous)throw std::runtime_error("Shared compare-swap denied or out of bounds");
        return *previous;
    }
    case Syscall::FileExists: { const auto path="/"+std::to_string(arg); return hasFileAccess(pid,path,FilePermission::Read)&&fs_.exists(path)?1:0; }
    case Syscall::OpenRead:
    case Syscall::OpenWrite: {
        const auto path=readGuestPath(it->second.machine,arg);
        const auto permission=call==Syscall::OpenRead?FilePermission::Read:FilePermission::Write;
        if(!hasFileAccess(pid,path,permission)) throw std::runtime_error("VFS access denied");
        if(!fs_.exists(path)) throw std::runtime_error("VFS file not found");
        if(it->second.nextHandle>255) throw std::runtime_error("Process file handle table exhausted");
        const auto handle=static_cast<std::uint8_t>(it->second.nextHandle++);
        it->second.openFiles.emplace(handle,OpenFile{path,0,call==Syscall::OpenWrite});
        return handle;
    }
    case Syscall::ReadByte: {
        auto file=it->second.openFiles.find(arg);
        if(file==it->second.openFiles.end()||file->second.writable) throw std::runtime_error("Invalid readable file handle");
        const auto data=fs_.readFile(file->second.path);
        if(file->second.position>=data.size()) return 0;
        return data[file->second.position++];
    }
    case Syscall::WriteByte: {
        if(static_cast<std::size_t>(arg)+1>=it->second.machine.ramSize()) throw std::runtime_error("Write descriptor outside process RAM");
        const auto handle=it->second.machine.readMemory(arg);
        const auto value=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+1));
        auto file=it->second.openFiles.find(handle);
        if(file==it->second.openFiles.end()||!file->second.writable) throw std::runtime_error("Invalid writable file handle");
        auto data=fs_.readFile(file->second.path);
        if(file->second.position>data.size()) data.resize(file->second.position,0);
        if(file->second.position==data.size()) data.push_back(value); else data[file->second.position]=value;
        fs_.writeFile(file->second.path,data); ++file->second.position; return 0;
    }
    case Syscall::CloseFile:
        if(!it->second.openFiles.erase(arg)) throw std::runtime_error("Invalid file handle");
        return 0;
    case Syscall::SendMessage: {
        if(static_cast<std::size_t>(arg)+3>it->second.machine.ramSize()) throw std::runtime_error("IPC send descriptor outside process RAM");
        const auto low=it->second.machine.readMemory(arg);
        const auto high=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+1));
        const auto length=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+2));
        const auto target=static_cast<std::uint32_t>(low|(static_cast<std::uint16_t>(high)<<8));
        if(static_cast<std::size_t>(arg)+3+length>it->second.machine.ramSize()) throw std::runtime_error("IPC payload outside process RAM");
        std::vector<std::uint8_t> payload; payload.reserve(length);
        for(std::size_t index=0;index<length;++index) payload.push_back(it->second.machine.readMemory(static_cast<std::uint16_t>(arg+3+index)));
        const auto destination=processes_.find(target);
        if(destination==processes_.end()||destination->second.info.state==ProcessState::Halted||
           destination->second.info.state==ProcessState::Faulted||destination->second.inbox.size()>=MaxInboxMessages)
            return 0;
        return sendMessage(pid,target,std::move(payload))?1:0;
    }
    case Syscall::ReceiveMessage: {
        if(static_cast<std::size_t>(arg)+5>it->second.machine.ramSize()) throw std::runtime_error("IPC receive descriptor outside process RAM");
        if(it->second.inbox.empty()) {
            it->second.info.state=ProcessState::Blocked;
            it->second.machine.retryCurrentInstruction();
            return 0;
        }
        const auto& message=it->second.inbox.front();
        if(static_cast<std::size_t>(arg)+5+message.payload.size()>it->second.machine.ramSize()) throw std::runtime_error("IPC receive buffer too small");
        const auto sender=message.sender;
        for(std::size_t byte=0;byte<4;++byte)
            it->second.machine.writeMemory(static_cast<std::uint16_t>(arg+byte),static_cast<std::uint8_t>((sender>>(8*byte))&0xFF));
        it->second.machine.writeMemory(static_cast<std::uint16_t>(arg+4),static_cast<std::uint8_t>(message.payload.size()));
        for(std::size_t index=0;index<message.payload.size();++index)
            it->second.machine.writeMemory(static_cast<std::uint16_t>(arg+5+index),message.payload[index]);
        it->second.inbox.pop_front();
        return 1;
    }
    }
    return 255;
}
std::uint8_t NexaKernel::readProcessMemory(std::uint32_t pid,std::uint16_t address) const {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    if(address>=it->second.machine.ramSize()) throw std::out_of_range("Process memory address out of range");
    return memory_.read(pid,address,MemoryAccess::Kernel);
}
void NexaKernel::writeProcessMemory(std::uint32_t pid,std::uint16_t address,std::uint8_t value) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    if(address>=it->second.machine.ramSize()) throw std::out_of_range("Process memory address out of range");
    memory_.write(pid,address,value,MemoryAccess::Kernel);
}
std::string NexaKernel::readGuestPath(const VM& machine,std::uint8_t address) const {
    std::string path;
    for(std::size_t pos=address;pos<machine.ramSize();++pos) {
        const auto ch=machine.readMemory(static_cast<std::uint16_t>(pos));
        if(ch==0) { if(path.empty()) throw std::runtime_error("Empty guest VFS path"); return path; }
        path.push_back(static_cast<char>(ch));
        if(path.size()>255) throw std::runtime_error("Guest VFS path too long");
    }
    throw std::runtime_error("Unterminated guest VFS path");
}
void NexaKernel::grantFileAccess(std::uint32_t pid,const std::string& path,FilePermission permissions) {
    if(!processes_.contains(pid)) throw std::out_of_range("Unknown process id");
    if(path.empty() || path.front()!='/' || path.find("..")!=std::string::npos)
        throw std::invalid_argument("File permission path must be an absolute normalized VFS path");
    if(!fs_.exists(path)) throw std::out_of_range("VFS path does not exist");
    filePermissions_[pid][path]=static_cast<std::uint8_t>(permissions);
}
bool NexaKernel::hasFileAccess(std::uint32_t pid,const std::string& path,FilePermission permission) const {
    if(!processes_.contains(pid)) throw std::out_of_range("Unknown process id");
    if(path.empty() || path.front()!='/' || path.find("..")!=std::string::npos) return false;
    const auto processIt=filePermissions_.find(pid);
    if(processIt==filePermissions_.end()) return false;
    const auto accessIt=processIt->second.find(path);
    if(accessIt==processIt->second.end()) return false;
    const auto requested=static_cast<std::uint8_t>(permission);
    return requested!=0 && (accessIt->second & requested)==requested;
}
}
