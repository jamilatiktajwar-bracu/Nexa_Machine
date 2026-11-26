#pragma once
#include "nexa/kernel.hpp"
#include "nexa/security.hpp"
#include <string>
#include <chrono>
namespace nexa {
class NexaShell {
public:
    explicit NexaShell(NexaKernel& kernel) : kernel_(kernel) { security_.registerPath("/",0755); lastActivity_=std::chrono::steady_clock::now(); }
    [[nodiscard]] std::string execute(const std::string& line);
private:
    NexaKernel& kernel_;
    std::string currentDirectory_{"/"};
    NexaSecurity security_;
    std::chrono::steady_clock::time_point lastActivity_{};
    static constexpr auto sessionTimeout_=std::chrono::minutes(15);
    [[nodiscard]] std::string resolvePath(const std::string& path) const;
    [[nodiscard]] bool canTraverse(const std::string& path) const;
    [[nodiscard]] bool canCreateIn(const std::string& path) const;
};
}
