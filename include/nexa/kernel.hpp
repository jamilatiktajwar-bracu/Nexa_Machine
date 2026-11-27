#pragma once
#include "nexa/executable.hpp"
#include "nexa/vfs.hpp"
#include "nexa/vm.hpp"
#include "nexa/memory_manager.hpp"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace nexa {
enum class ProcessState { Ready, Running, Blocked, Stopped, Halted, Faulted };
struct ProcessInfo {
    std::uint32_t pid{};
    std::string name;
    ProcessState state{ProcessState::Ready};
    std::uint64_t ticks{};
    std::string faultReason;
    std::uint8_t priority{0};
    std::size_t pendingMessages{0};
    std::uint8_t effectivePriority{0};
    std::uint64_t readyWaitTicks{0};
    std::optional<std::uint8_t> exitCode{};
    std::string terminationReason;
    std::uint64_t instructionBudget{0};
    std::size_t pendingSignals{0};
};
enum class Syscall : std::uint8_t { Exit=0, Write=1, Read=2, GetPid=3, Yield=4, FileExists=5, OpenRead=6, OpenWrite=7, ReadByte=8, WriteByte=9, CloseFile=10, SendMessage=11, ReceiveMessage=12, GetSignal=13, SemaphoreWait=14, SemaphoreSignal=15, MutexLock=16, MutexUnlock=17, SharedAttach=18, SharedDetach=19, SharedRead=20, SharedWrite=21, SharedCreate=22, SharedAtomicAdd=23, SharedCompareSwap=24, SharedAttachReadOnly=25, SharedAtomicAdd16=26, SharedCompareSwap16=27 };
enum class ProcessSignal : std::uint8_t { Terminate=1, Kill=2, Stop=3, Continue=4, User1=10, User2=12 };
struct IpcMessage { std::uint32_t sender{}; std::vector<std::uint8_t> payload; };
struct SharedMemoryInfo { std::uint8_t id{}; std::size_t size{}; std::size_t attachments{}; };
enum class FilePermission : std::uint8_t { None=0, Read=1, Write=2, Execute=4 };
class NexaKernel {
public:
    explicit NexaKernel(std::size_t ramBytes=VM::DefaultRamSize);
    std::uint32_t spawn(const std::string& name, const Executable& image);
    bool reapProcess(std::uint32_t pid);
    bool terminateProcess(std::uint32_t pid);
    bool sendSignal(std::uint32_t pid,ProcessSignal signal);
    [[nodiscard]] std::optional<ProcessSignal> receiveSignal(std::uint32_t pid);
    [[nodiscard]] bool hasDeadlock() const noexcept;
    void setPriority(std::uint32_t pid,std::uint8_t priority);
    void setInstructionBudget(std::uint32_t pid,std::uint64_t instructions);
    void setTimeQuantum(std::size_t instructions);
    void setPriorityAgingThreshold(std::uint64_t ticks);
    [[nodiscard]] std::uint64_t priorityAgingThreshold() const noexcept{return priorityAgingThreshold_;}
    [[nodiscard]] std::size_t timeQuantum() const noexcept{return timeQuantum_;}
    [[nodiscard]] std::uint64_t timerInterval() const noexcept{return timerInterval_;}
    void setTimerInterval(std::uint64_t ticks);
    [[nodiscard]] std::uint64_t systemTicks() const noexcept{return systemTicks_;}
    [[nodiscard]] std::uint64_t timerInterrupts() const noexcept{return timerInterrupts_;}
    [[nodiscard]] std::uint64_t pendingTimerInterrupts() const noexcept{return pendingTimerInterrupts_;}
    bool acknowledgeTimerInterrupt() noexcept;
    std::uint8_t createSemaphore(std::uint8_t initial,std::uint8_t maximum);
    bool semaphoreWait(std::uint32_t pid,std::uint8_t id);
    bool semaphoreSignal(std::uint8_t id);
    std::uint8_t createMutex();
    bool mutexLock(std::uint32_t pid,std::uint8_t id);
    bool mutexUnlock(std::uint32_t pid,std::uint8_t id);
    bool sendMessage(std::uint32_t from,std::uint32_t to,std::vector<std::uint8_t> payload);
    [[nodiscard]] std::optional<IpcMessage> receiveMessage(std::uint32_t pid);
    bool scheduleOne();
    void run(std::size_t maxSlices=100000);
    [[nodiscard]] std::vector<ProcessInfo> processes() const;
    [[nodiscard]] std::uint8_t effectivePriority(std::uint32_t pid) const;
    [[nodiscard]] std::string syncDiagnostics() const;
    std::uint8_t createSharedMemory(std::size_t bytes);
    bool destroySharedMemory(std::uint8_t id);
    bool attachSharedMemory(std::uint32_t pid,std::uint8_t id);
    bool detachSharedMemory(std::uint32_t pid,std::uint8_t id);
    bool mapSharedMemory(std::uint32_t pid,std::uint8_t id,std::uint8_t basePage,bool writable=true);
    bool unmapSharedMemory(std::uint32_t pid,std::uint8_t id);
    [[nodiscard]] std::optional<std::uint8_t> readSharedMemory(std::uint32_t pid,std::uint8_t id,std::size_t offset) const;
    bool writeSharedMemory(std::uint32_t pid,std::uint8_t id,std::size_t offset,std::uint8_t value);
    [[nodiscard]] std::optional<std::uint8_t> sharedAtomicAdd(std::uint32_t pid,std::uint8_t id,std::size_t offset,std::uint8_t delta);
    [[nodiscard]] std::optional<std::uint8_t> sharedCompareSwap(std::uint32_t pid,std::uint8_t id,std::size_t offset,std::uint8_t expected,std::uint8_t replacement);
    [[nodiscard]] std::optional<std::uint16_t> sharedAtomicAdd16(std::uint32_t pid,std::uint8_t id,std::size_t offset,std::uint16_t delta);
    [[nodiscard]] std::optional<std::uint16_t> sharedCompareSwap16(std::uint32_t pid,std::uint8_t id,std::size_t offset,std::uint16_t expected,std::uint16_t replacement);
    [[nodiscard]] std::vector<SharedMemoryInfo> sharedMemoryInfo() const;
    [[nodiscard]] std::uint32_t currentPid() const noexcept { return currentPid_; }
    [[nodiscard]] VirtualFileSystem& filesystem() noexcept { return fs_; }
    [[nodiscard]] VirtualMemoryManager& memoryManager() noexcept { return memory_; }
    [[nodiscard]] const VirtualMemoryManager& memoryManager() const noexcept { return memory_; }
    [[nodiscard]] std::string takeConsoleOutput();
    std::uint8_t syscall(std::uint32_t pid, Syscall call, std::uint8_t argument=0);
    [[nodiscard]] std::uint8_t readProcessMemory(std::uint32_t pid, std::uint16_t address) const;
    void writeProcessMemory(std::uint32_t pid, std::uint16_t address, std::uint8_t value);
    void grantFileAccess(std::uint32_t pid, const std::string& path, FilePermission permissions);
    [[nodiscard]] bool hasFileAccess(std::uint32_t pid, const std::string& path, FilePermission permission) const;
private:
    struct OpenFile { std::string path; std::size_t position{}; bool writable{}; };
    struct Process { ProcessInfo info; VM machine; std::set<std::uint8_t> sharedAttachments; std::map<std::uint8_t,std::uint8_t> sharedMappings; std::map<std::uint8_t,bool> sharedMappingWritable; std::map<std::uint8_t, OpenFile> openFiles; std::uint16_t nextHandle{1}; std::deque<IpcMessage> inbox; std::deque<ProcessSignal> signals; std::deque<std::uint8_t> semaphoreGrants,mutexGrants; bool yieldRequested{false}; std::uint64_t readyWaitTicks{0}; };
    struct Semaphore { std::uint16_t count{},maximum{}; std::deque<std::uint32_t> waiters; };
    struct Mutex { std::optional<std::uint32_t> owner{}; std::deque<std::uint32_t> waiters; };
    std::size_t ramBytes_;
    std::uint32_t nextPid_{1}, currentPid_{0};
    std::map<std::uint32_t, Process> processes_;
    std::uint32_t cursor_{0};
    std::size_t timeQuantum_{1};
    std::uint64_t systemTicks_{0},timerInterval_{0},timerInterrupts_{0},pendingTimerInterrupts_{0},priorityAgingThreshold_{0};
    static constexpr std::size_t MaxInboxMessages=64,MaxIpcPayload=251,MaxPendingSignals=32;
    VirtualFileSystem fs_;
    VirtualMemoryManager memory_;
    std::string output_;
    std::map<std::uint32_t, std::map<std::string, std::uint8_t>> filePermissions_;
    std::map<std::uint8_t,Semaphore> semaphores_;
    std::map<std::uint8_t,Mutex> mutexes_;
    struct SharedSegment { std::size_t size{}; std::vector<std::uint16_t> frames; std::set<std::uint32_t> attached; };
    std::map<std::uint8_t,SharedSegment> sharedSegments_;
    std::uint16_t nextSemaphore_{1},nextMutex_{1},nextSharedSegment_{1};
    static constexpr std::size_t MaxSyncObjects=255;
    [[nodiscard]] std::string readGuestPath(const VM& machine, std::uint8_t address) const;
    void cleanupSynchronizationState(std::uint32_t pid);
    void releaseOwnedMutexes(std::uint32_t pid);
    void releaseSharedAttachments(std::uint32_t pid);
};
}
