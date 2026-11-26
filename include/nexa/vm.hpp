#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace nexa {

enum class Opcode : std::uint8_t {
    LoadImmediate, // A <- immediate
    LoadBImmediate, // B <- immediate
    Add,          // A <- A + B (wraps to 8 bits)
    Store,        // RAM[address] <- A
    Load,         // A <- RAM[address]
    Print,        // print A
    Halt
};

struct Instruction {
    Opcode opcode{};
    std::uint8_t operand{};
};

class VM {
public:
    static constexpr std::size_t RamSize = 256;

    void loadProgram(std::vector<Instruction> program);
    void run();

    [[nodiscard]] std::uint8_t accumulator() const noexcept { return a_; }
    [[nodiscard]] std::uint8_t secondRegister() const noexcept { return b_; }
    [[nodiscard]] std::uint8_t readMemory(std::uint8_t address) const noexcept {
        return ram_[address];
    }

private:
    std::array<std::uint8_t, RamSize> ram_{};
    std::uint8_t a_{0};
    std::uint8_t b_{0};
    std::size_t pc_{0};
    std::vector<Instruction> program_;
    bool halted_{false};

    void step();
};

} // namespace nexa
