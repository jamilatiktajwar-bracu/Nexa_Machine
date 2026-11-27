#include "nexa/platform.hpp"
#include <bit>
#include <sstream>
#if defined(_WIN32)
#define NEXA_OS_NAME "Windows"
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define NEXA_OS_NAME "iOS"
#else
#define NEXA_OS_NAME "macOS"
#endif
#elif defined(__linux__)
#define NEXA_OS_NAME "Linux"
#elif defined(__FreeBSD__)
#define NEXA_OS_NAME "FreeBSD"
#else
#define NEXA_OS_NAME "Unknown"
#endif
#if defined(__x86_64__) || defined(_M_X64)
#define NEXA_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#define NEXA_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define NEXA_ARCH_NAME "ARM64"
#elif defined(__arm__) || defined(_M_ARM)
#define NEXA_ARCH_NAME "ARM"
#else
#define NEXA_ARCH_NAME "Unknown"
#endif
#if defined(__clang__)
#define NEXA_COMPILER "Clang " __clang_version__
#elif defined(__GNUC__)
#define NEXA_COMPILER "GCC " __VERSION__
#elif defined(_MSC_VER)
#define NEXA_COMPILER "MSVC"
#else
#define NEXA_COMPILER "Unknown compiler"
#endif
namespace nexa {
PlatformInfo platformInfo() {
#if defined(NDEBUG)
 const char* build = "Release";
#else
 const char* build = "Debug";
#endif
 const bool mobile = std::string(NEXA_OS_NAME) == "iOS";
 const bool desktop = !mobile && (std::string(NEXA_OS_NAME) == "Windows" || std::string(NEXA_OS_NAME) == "macOS" || std::string(NEXA_OS_NAME) == "Linux");
 const char* endian = std::endian::native == std::endian::little ? "Little-endian" : std::endian::native == std::endian::big ? "Big-endian" : "Mixed-endian";
 return {NEXA_OS_NAME, NEXA_ARCH_NAME, NEXA_COMPILER, build, endian, desktop, mobile};
}
std::string platformSummary() {
 const auto p = platformInfo();
 std::ostringstream out;
 out << "Host OS: " << p.operatingSystem << "\nArchitecture: " << p.architecture
 << "\nCompiler: " << p.compiler << "\nBuild: " << p.buildType
 << "\nByte order: " << p.byteOrder << "\nDesktop: " << (p.desktop ? "yes" : "no")
 << "\nMobile: " << (p.mobile ? "yes" : "no");
 return out.str();
}
}
