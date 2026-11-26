#include "nexa/assembler.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

namespace nexa {
namespace {
std::string trim(std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}
std::string upper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}
std::vector<std::string> splitOperands(const std::string& text) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, ',')) parts.push_back(trim(part));
    return parts;
}
unsigned parseByte(std::string token, std::size_t line) {
    token = trim(token);
    try {
        std::size_t consumed = 0;
        const int value = std::stoi(token, &consumed, 0);
        if (consumed != token.size() || value < 0 || value > 255)
            throw std::runtime_error("value must be between 0 and 255");
        return static_cast<unsigned>(value);
    } catch (const std::exception&) {
        throw std::runtime_error("Assembly line " + std::to_string(line) +
                                 ": invalid byte value '" + token + "'");
    }
}
std::string unbracket(std::string operand) {
    operand = trim(operand);
    if (operand.size() >= 2 && operand.front() == '[' && operand.back() == ']')
        return trim(operand.substr(1, operand.size() - 2));
    return operand;
}
}
std::vector<Instruction> parseAssembly(const std::string& source) {
    std::vector<Instruction> program;
    std::stringstream input(source);
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) line.erase(comment);
        line = trim(line);
        if (line.empty()) continue;

        std::stringstream instructionStream(line);
        std::string mnemonic, operandText;
        instructionStream >> mnemonic;
        std::getline(instructionStream, operandText);
        mnemonic = upper(mnemonic);
        const auto operands = splitOperands(trim(operandText));

        try {
            if (mnemonic == "HALT") {
                if (!trim(operandText).empty()) throw std::runtime_error("HALT takes no operands");
                program.push_back({Opcode::Halt, 0});
            } else if (mnemonic == "PRINT") {
                if (operands.size() != 1 || upper(operands[0]) != "A")
                    throw std::runtime_error("syntax: PRINT A");
                program.push_back({Opcode::Print, 0});
            } else if (mnemonic == "ADD") {
                if (operands.size() != 2 || upper(operands[0]) != "A" || upper(operands[1]) != "B")
                    throw std::runtime_error("syntax: ADD A, B");
                program.push_back({Opcode::Add, 0});
            } else if (mnemonic == "LOAD") {
                if (operands.size() != 2) throw std::runtime_error("syntax: LOAD A, value or LOAD A, [address]");
                const auto reg = upper(operands[0]);
                if (reg == "A" && operands[1].front() == '[') {
                    program.push_back({Opcode::Load, static_cast<std::uint8_t>(parseByte(unbracket(operands[1]), lineNumber))});
                } else if (reg == "A") {
                    program.push_back({Opcode::LoadImmediate, static_cast<std::uint8_t>(parseByte(operands[1], lineNumber))});
                } else if (reg == "B") {
                    program.push_back({Opcode::LoadBImmediate, static_cast<std::uint8_t>(parseByte(operands[1], lineNumber))});
                } else {
                    throw std::runtime_error("register must be A or B");
                }
            } else if (mnemonic == "STORE") {
                if (operands.size() != 1) throw std::runtime_error("syntax: STORE [address]");
                program.push_back({Opcode::Store, static_cast<std::uint8_t>(parseByte(unbracket(operands[0]), lineNumber))});
            } else {
                throw std::runtime_error("unknown instruction '" + mnemonic + "'");
            }
        } catch (const std::exception& error) {
            throw std::runtime_error("Assembly line " + std::to_string(lineNumber) + ": " + error.what());
        }
    }
    if (program.empty()) throw std::runtime_error("Assembly program is empty");
    return program;
}
}
