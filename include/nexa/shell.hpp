#pragma once
#include "nexa/kernel.hpp"
#include "nexa/security.hpp"
#include <string>
namespace nexa {
class NexaShell {
public:
    explicit NexaShell(NexaKernel& kernel) : kernel_(kernel) {}
    [[nodiscard]] std::string execute(const std::string& line);
private:
    NexaKernel& kernel_;
    std::string currentDirectory_{"/"};
    NexaSecurity security_;
    [[nodiscard]] std::string resolvePath(const std::string& path) const;
};
}
