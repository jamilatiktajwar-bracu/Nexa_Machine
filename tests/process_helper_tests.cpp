#include "test_process.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc == 5 && std::string(argv[1]) == "--child") {
        std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
        if (!output) return 31;
        output << argv[3] << '\n' << argv[4];
        output.flush();
        return output ? 37 : 32;
    }

    const auto root = fs::temp_directory_path() / "nexa process helper test";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root);

    const auto resultFile = root / "child output.txt";
    const std::string quotedArgument = "embedded \"quoted\" text with spaces";
    const std::string trailingBackslash = "ends with backslash\\";
    const int status = nexa_test::runChildProcess(
        argv[0], {"--child", resultFile.string(), quotedArgument, trailingBackslash});

    if (status != 37) {
        std::cerr << "child process returned unexpected status: " << status << '\n';
        fs::remove_all(root, error);
        return 1;
    }

    std::ifstream input(resultFile, std::ios::binary);
    std::string observedQuoted;
    std::string observedTrailing;
    std::getline(input, observedQuoted);
    std::getline(input, observedTrailing);
    if (!input && !input.eof()) {
        std::cerr << "failed reading child process argument capture\n";
        fs::remove_all(root, error);
        return 2;
    }
    if (observedQuoted != quotedArgument || observedTrailing != trailingBackslash) {
        std::cerr << "child process argument round-trip mismatch\n"
                  << "quoted expected: " << quotedArgument << '\n'
                  << "quoted actual:   " << observedQuoted << '\n'
                  << "trailing expected: " << trailingBackslash << '\n'
                  << "trailing actual:   " << observedTrailing << '\n';
        fs::remove_all(root, error);
        return 3;
    }

    fs::remove_all(root, error);
    std::cout << "Portable child-process quoting tests passed\n";
    return 0;
}
