#pragma once
#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <vector>
namespace nexa {
enum class Opcode : std::uint8_t { LoadImmediate, LoadBImmediate, Add, Sub, Inc, Dec, Compare, CompareImmediate, Store, Load, Print, Input, Output, Jump, JumpZero, JumpNotZero, PushA, PushB, PopA, PopB, Call, Return, Halt };
struct Instruction { Opcode opcode{}; std::uint16_t operand{}; };
class VM {
public:
 static constexpr std::size_t DefaultRamSize=256;
 static constexpr std::uint16_t KeyboardPort=0, DisplayPort=1;
 explicit VM(std::size_t ramSize=DefaultRamSize);
 void loadProgram(std::vector<Instruction>);
 void run(std::size_t instructionBudget=10000000);
 bool stepInstruction();
 void reset();
 void saveState(const std::string& path)const;
 void loadState(const std::string& path);
 void queueKeyboardInput(const std::string& text);
 void setBreakpoint(std::size_t address);
 void clearBreakpoint(std::size_t address);
 void clearBreakpoints();
 [[nodiscard]] bool hasBreakpoint(std::size_t address)const;
 [[nodiscard]] const std::set<std::size_t>& breakpoints()const noexcept{return breakpoints_;}
 [[nodiscard]] const std::string& deviceOutput()const noexcept{return deviceOutput_;}
 void clearDeviceOutput(){deviceOutput_.clear();}
 [[nodiscard]] std::uint8_t accumulator()const noexcept{return a_;}
 [[nodiscard]] std::uint8_t secondRegister()const noexcept{return b_;}
 [[nodiscard]] std::uint8_t readMemory(std::uint16_t)const noexcept;
 [[nodiscard]] std::size_t ramSize()const noexcept{return ram_.size();}
 [[nodiscard]] std::size_t instructionsExecuted()const noexcept{return instructionCount_;}
 [[nodiscard]] bool halted()const noexcept{return halted_;}
 [[nodiscard]] std::size_t programCounter()const noexcept{return pc_;}
 [[nodiscard]] std::size_t stackDepth()const noexcept{return stack_.size();}
private:
 std::vector<std::uint8_t>ram_;
 std::uint8_t a_{0},b_{0};
 std::size_t pc_{0},instructionCount_{0};
 std::vector<Instruction>program_;
 std::vector<std::uint16_t>stack_;
 std::deque<std::uint8_t>keyboard_;
 std::string deviceOutput_;
 std::set<std::size_t>breakpoints_;
 bool halted_{false},zeroFlag_{false};
 void step();
};
}
