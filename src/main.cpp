#include "nexa/assembler.hpp"
#include "nexa/vm.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
const char* demo = R"ASM(
# Nexa assembly demo: 19 + 23 = 42
LOAD A, 19
LOAD B, 23
ADD A, B
PRINT A
HALT
)ASM";
}
int main(int argc, char** argv) {
    std::cout << "Nexa Machine VM 0.2\n";
    try {
        std::string source;
        if (argc > 2) {
            std::cerr << "Usage: nexa [program.nexa]\n";
            return 2;
        }
        if (argc == 2) {
            std::ifstream file(argv[1]);
            if (!file) {
                std::cerr << "Cannot open assembly file: " << argv[1] << '\n';
                return 2;
            }
            source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        } else {
            source = demo;
            std::cout << "Running built-in demo (pass a .nexa file to run your own)\n";
        }

        nexa::VM vm;
        vm.loadProgram(nexa::parseAssembly(source));
        vm.run();
        std::cout << "VM halted.\n";
    } catch (const std::exception& error) {
        std::cerr << "Nexa error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
