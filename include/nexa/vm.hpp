#pragma once
#include <cstdint>
#include <vector>
namespace nexa {
enum class Opcode : std::uint8_t { LoadImmediate, LoadBImmediate, Add, Sub, Inc, Dec, Compare, CompareImmediate, Store, Load, Print, Jump, JumpZero, JumpNotZero, PushA, PushB, PopA, PopB, Call, Return, Halt };
struct Instruction { Opcode opcode{}; std::uint16_t operand{}; };
class VM {
public:
 static constexpr std::size_t DefaultRamSize=256;
 explicit VM(std::size_t ramSize=DefaultRamSize);
 void loadProgram(std::vector<Instruction>); void run(); void reset();
 [[nodiscard]] std::uint8_t accumulator()const noexcept{return a_;}
 [[nodiscard]] std::uint8_t secondRegister()const noexcept{return b_;}
 [[nodiscard]] std::uint8_t readMemory(std::uint16_t)const noexcept;
 [[nodiscard]] std::size_t ramSize()const noexcept{return ram_.size();}
 [[nodiscard]] std::size_t instructionsExecuted()const noexcept{return instructionCount_;}
 [[nodiscard]] bool halted()const noexcept{return halted_;}
 [[nodiscard]] std::size_t programCounter()const noexcept{return pc_;}
 [[nodiscard]] std::size_t stackDepth()const noexcept{return stack_.size();}
private:
 std::vector<std::uint8_t>ram_;std::uint8_t a_{0},b_{0};std::size_t pc_{0},instructionCount_{0};
 std::vector<Instruction>program_;std::vector<std::uint16_t>stack_;bool halted_{false},zeroFlag_{false};
 void step();
};
}