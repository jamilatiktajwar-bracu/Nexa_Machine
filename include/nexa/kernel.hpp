#pragma once
#include "nexa/executable.hpp"
#include "nexa/vfs.hpp"
#include "nexa/vm.hpp"
#include "nexa/memory_manager.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace nexa {
enum class ProcessState { Ready, Running, Halted, Faulted };
struct ProcessInfo {
    std::uint32_t pid{};
    std::string name;
    ProcessState state{ProcessState::Ready};
    std::uint64_t ticks{};
    std::string faultReason;
};
enum class Syscall : std::uint8_t { Exit=0, Write=1, Read=2, GetPid=3, Yield=4, FileExists=5, OpenRead=6, OpenWrite=7, ReadByte=8, WriteByte=9, CloseFile=10 };
enum class FilePermission : std::uint8_t { None=0, Read=1, Write=2, Execute=4 };
class NexaKernel {
public:
    explicit NexaKernel(std::size_t ramBytes=VM::DefaultRamSize);
    std::uint32_t spawn(const std::string& name, const Executable& image);
    bool reapProcess(std::uint32_t pid);
    bool scheduleOne();
    void run(std::size_t maxTicks=100000);
    [[nodiscard]] std::vector<ProcessInfo> processes() const;
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
    struct Process { ProcessInfo info; VM machine; std::map<std::uint8_t, OpenFile> openFiles; std::uint16_t nextHandle{1}; };
    std::size_t ramBytes_;
    std::uint32_t nextPid_{1}, currentPid_{0};
    std::map<std::uint32_t, Process> processes_;
    std::uint32_t cursor_{0};
    VirtualFileSystem fs_;
    VirtualMemoryManager memory_;
    std::string output_;
    std::map<std::uint32_t, std::map<std::string, std::uint8_t>> filePermissions_;
    [[nodiscard]] std::string readGuestPath(const VM& machine, std::uint8_t address) const;
};
}
