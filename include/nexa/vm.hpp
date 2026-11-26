#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <utility>
#include <set>
#include <string>
#include <map>
#include <vector>
namespace nexa {
enum class Opcode : std::uint8_t { LoadImmediate, LoadBImmediate, Add, Sub, Inc, Dec, Compare, CompareImmediate, Store, Load, Print, Input, Output, Syscall, Jump, JumpZero, JumpNotZero, PushA, PushB, PopA, PopB, Call, Return, Interrupt, InterruptReturn, Halt };
struct Instruction { Opcode opcode{}; std::uint16_t operand{}; };
enum class PrivilegeMode { User, Kernel };
class VM {
public:
 static constexpr std::size_t DefaultRamSize=256, ScreenWidth=64, ScreenHeight=32;
 static constexpr std::uint16_t KeyboardPort=0, DisplayPort=1, GraphicsXPort=2, GraphicsYPort=3, GraphicsColorPort=4, GraphicsDrawPort=5, GraphicsClearPort=6;
 explicit VM(std::size_t ramSize=DefaultRamSize);
 void loadProgram(std::vector<Instruction>);
 void setSyscallHandler(std::function<std::uint8_t(std::uint8_t,std::uint8_t)> handler){syscallHandler_=std::move(handler);}
 [[nodiscard]] PrivilegeMode privilegeMode()const noexcept{return privilegeMode_;}
 void setMemoryAccessors(std::function<std::uint8_t(std::uint16_t)> reader,std::function<void(std::uint16_t,std::uint8_t)> writer){memoryReader_=std::move(reader);memoryWriter_=std::move(writer);}
 void setInstructionFetchGuard(std::function<void(std::uint16_t)> guard){instructionFetchGuard_=std::move(guard);}
 void setInstructionReader(std::function<Instruction(std::uint16_t)> reader){instructionReader_=std::move(reader);}
 void setProgramCounter(std::uint16_t address);
 void run(std::size_t instructionBudget=10000000);
 bool stepInstruction();
 void reset();
 void saveState(const std::string& path)const;
 void loadState(const std::string& path);
 void queueKeyboardInput(const std::string& text);
 void setBreakpoint(std::size_t address);
 void clearBreakpoint(std::size_t address);
 void clearBreakpoints();
 void setInterruptVector(std::uint8_t vector,std::uint16_t address);
 void triggerInterrupt(std::uint8_t vector);
 [[nodiscard]] bool hasBreakpoint(std::size_t address)const;
 [[nodiscard]] const std::set<std::size_t>& breakpoints()const noexcept{return breakpoints_;}
 [[nodiscard]] const std::string& deviceOutput()const noexcept{return deviceOutput_;}
 void clearDeviceOutput(){deviceOutput_.clear();}
 [[nodiscard]] const std::vector<std::uint8_t>& framebuffer()const noexcept{return framebuffer_;}
 [[nodiscard]] bool pixel(std::size_t x,std::size_t y)const noexcept;
 void drawPixel(std::size_t x,std::size_t y,bool white=true)noexcept;
 void writeMemory(std::uint16_t address,std::uint8_t value);
 void setWatchpoint(std::uint16_t address);
 void clearWatchpoint(std::uint16_t address);
 [[nodiscard]] bool watchpointHit()const noexcept{return watchpointHit_;}
 [[nodiscard]] std::uint16_t watchedAddress()const noexcept{return watchedAddress_;}
 [[nodiscard]] std::uint8_t accumulator()const noexcept{return a_;}
 [[nodiscard]] std::uint8_t secondRegister()const noexcept{return b_;}
 [[nodiscard]] std::uint8_t readMemory(std::uint16_t)const;
 [[nodiscard]] std::size_t ramSize()const noexcept{return ram_.size();}
 [[nodiscard]] std::size_t instructionsExecuted()const noexcept{return instructionCount_;}
 [[nodiscard]] bool halted()const noexcept{return halted_;}
 [[nodiscard]] std::size_t programCounter()const noexcept{return pc_;}
 [[nodiscard]] std::size_t stackDepth()const noexcept{return stack_.size();}
private:
 std::vector<std::uint8_t>ram_,framebuffer_;
 std::uint8_t a_{0},b_{0},graphicsX_{0},graphicsY_{0},graphicsColor_{1};
 std::size_t pc_{0},instructionCount_{0};
 std::vector<Instruction>program_;
 std::vector<std::uint16_t>stack_;
 std::deque<std::uint8_t>keyboard_;
 std::string deviceOutput_;
 std::function<std::uint8_t(std::uint8_t,std::uint8_t)> syscallHandler_;
 std::function<std::uint8_t(std::uint16_t)> memoryReader_;
 std::function<void(std::uint16_t,std::uint8_t)> memoryWriter_; 
 std::function<void(std::uint16_t)> instructionFetchGuard_;
 std::function<Instruction(std::uint16_t)> instructionReader_;
 std::set<std::size_t>breakpoints_;
 std::map<std::uint8_t,std::uint16_t>interruptVectors_;
 std::uint16_t watchedAddress_{0};bool watchpointEnabled_{false},watchpointHit_{false};
 bool halted_{false},zeroFlag_{false};
 PrivilegeMode privilegeMode_{PrivilegeMode::User};
 void step();
};
}
