#pragma once

#include "nexa/vm.hpp"

#include <string>
#include <vector>

namespace nexa {
std::vector<Instruction> parseAssembly(const std::string& source);
}
