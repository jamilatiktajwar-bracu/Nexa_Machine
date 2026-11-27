#pragma once
#include <string>
namespace nexa {
struct PlatformInfo { std::string operatingSystem, architecture, compiler, buildType, byteOrder; bool desktop, mobile; };
PlatformInfo platformInfo();
std::string platformSummary();
}
