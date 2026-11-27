#include "nexa/platform.hpp"
#include <cassert>
#include <iostream>
int main() {
 const auto p=nexa::platformInfo();
 assert(!p.operatingSystem.empty() && !p.architecture.empty() && !p.compiler.empty() && !p.buildType.empty());
 assert(p.byteOrder=="Little-endian" || p.byteOrder=="Big-endian" || p.byteOrder=="Mixed-endian");
 const auto s=nexa::platformSummary();
 assert(s.find("Host OS: ")==0 && s.find("Architecture: ")!=std::string::npos);
 std::cout<<"Platform diagnostics tests passed\n";
}
