#pragma once
#include "nexa/kernel.hpp"
#include <string>
namespace nexa {
class NexaShell {
public:
    explicit NexaShell(NexaKernel& kernel) : kernel_(kernel) {}
    [[nodiscard]] std::string execute(const std::string& line);
private:
    NexaKernel& kernel_;
};
}
