#pragma once
#include "nexa/executable.hpp"
#include "nexa/vfs.hpp"
#include "nexa/vm.hpp"
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
enum class Syscall : std::uint8_t { Exit=0, Write=1, Read=2, GetPid=3, Yield=4, FileExists=5 };
class NexaKernel {
public:
    explicit NexaKernel(std::size_t ramBytes=VM::DefaultRamSize);
    std::uint32_t spawn(const std::string& name, const Executable& image);
    bool scheduleOne();
    void run(std::size_t maxTicks=100000);
    [[nodiscard]] std::vector<ProcessInfo> processes() const;
    [[nodiscard]] std::uint32_t currentPid() const noexcept { return currentPid_; }
    [[nodiscard]] VirtualFileSystem& filesystem() noexcept { return fs_; }
    [[nodiscard]] std::string takeConsoleOutput();
    std::uint8_t syscall(std::uint32_t pid, Syscall call, std::uint8_t argument=0);
    [[nodiscard]] std::uint8_t readProcessMemory(std::uint32_t pid, std::uint16_t address) const;
    void writeProcessMemory(std::uint32_t pid, std::uint16_t address, std::uint8_t value);
private:
    struct Process { ProcessInfo info; VM machine; };
    std::size_t ramBytes_;
    std::uint32_t nextPid_{1}, currentPid_{0};
    std::map<std::uint32_t, Process> processes_;
    std::uint32_t cursor_{0};
    VirtualFileSystem fs_;
    std::string output_;
};
}
