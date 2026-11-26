#pragma once
#include "nexa/vm.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace nexa {
struct Executable { std::uint16_t entryPoint{0}; std::vector<Instruction> code; };
void saveExecutable(const Executable&,const std::string& path);
Executable loadExecutable(const std::string& path);
}
