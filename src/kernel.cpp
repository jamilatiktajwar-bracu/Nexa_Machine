#include "nexa/kernel.hpp"
#include <algorithm>
#include <exception>
#include <stdexcept>
namespace nexa {
NexaKernel::NexaKernel(std::size_t n):ramBytes_(n){VM check(n);(void)check;}
std::uint32_t NexaKernel::spawn(const std::string& name,const Executable& image) {
    if(name.empty()) throw std::invalid_argument("Process name cannot be empty");
    if(image.code.empty()||image.entryPoint>=image.code.size()) throw std::invalid_argument("Invalid process executable");
    const auto pid=nextPid_++;
    Process p{ProcessInfo{pid,name,ProcessState::Ready,0,{}},VM(ramBytes_)};
    p.machine.loadProgram(image.code);
    p.machine.setProgramCounter(image.entryPoint);
    p.machine.setSyscallHandler([this,pid](std::uint8_t service,std::uint8_t argument) {
        if(service>static_cast<std::uint8_t>(Syscall::FileExists)) throw std::runtime_error("Unknown kernel syscall");
        return syscall(pid,static_cast<Syscall>(service),argument);
    });
    processes_.emplace(pid,std::move(p));
    return pid;
}
bool NexaKernel::scheduleOne() {
    if(processes_.empty()) return false;
    std::vector<std::uint32_t> ids;
    for(const auto& [id,p]:processes_) if(p.info.state==ProcessState::Ready) ids.push_back(id);
    if(ids.empty()) return false;
    auto it=std::find_if(ids.begin(),ids.end(),[&](auto id){return id>cursor_;});
    const auto pid=it==ids.end()?ids.front():*it;
    cursor_=pid; currentPid_=pid;
    auto& proc=processes_.at(pid);
    proc.info.state=ProcessState::Running;
    try {
        proc.machine.stepInstruction();
        ++proc.info.ticks;
        output_+=proc.machine.deviceOutput();
        proc.machine.clearDeviceOutput();
        proc.info.state=proc.machine.halted()?ProcessState::Halted:ProcessState::Ready;
    } catch(const std::exception& e) {
        proc.info.state=ProcessState::Faulted;
        proc.info.faultReason=e.what();
        output_+=proc.machine.deviceOutput();
        proc.machine.clearDeviceOutput();
    } catch(...) {
        proc.info.state=ProcessState::Faulted;
        proc.info.faultReason="Unknown process fault";
    }
    return true;
}
void NexaKernel::run(std::size_t maxTicks) { for(std::size_t n=0;n<maxTicks&&scheduleOne();++n) {} }
std::vector<ProcessInfo> NexaKernel::processes() const {
    std::vector<ProcessInfo> result;
    for(const auto& [id,p]:processes_) result.push_back(p.info);
    return result;
}
std::string NexaKernel::takeConsoleOutput() { auto s=std::move(output_); output_.clear(); return s; }
std::uint8_t NexaKernel::syscall(std::uint32_t pid,Syscall call,std::uint8_t arg) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    switch(call) {
    case Syscall::Exit: it->second.info.state=ProcessState::Halted; return 0;
    case Syscall::Write: output_.push_back(static_cast<char>(arg)); return 0;
    case Syscall::Read: return it->second.machine.readMemory(arg);
    case Syscall::GetPid: return static_cast<std::uint8_t>(pid&255);
    case Syscall::Yield: return 0;
    case Syscall::FileExists: return fs_.exists("/"+std::to_string(arg))?1:0;
    }
    return 255;
}
std::uint8_t NexaKernel::readProcessMemory(std::uint32_t pid,std::uint16_t address) const {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    if(address>=it->second.machine.ramSize()) throw std::out_of_range("Process memory address out of range");
    return it->second.machine.readMemory(address);
}
void NexaKernel::writeProcessMemory(std::uint32_t pid,std::uint16_t address,std::uint8_t value) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    if(address>=it->second.machine.ramSize()) throw std::out_of_range("Process memory address out of range");
    it->second.machine.writeMemory(address,value);
}
}
