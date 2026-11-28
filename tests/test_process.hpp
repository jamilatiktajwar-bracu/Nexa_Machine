#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdlib>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#endif

namespace nexa_test {

#ifdef _WIN32
inline std::wstring quoteWindowsArgument(const std::wstring& value) {
    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t ch : value) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(ch);
        } else {
            quoted.append(backslashes, L'\\');
            quoted.push_back(ch);
        }
        backslashes = 0;
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}
#else
inline std::string quotePosixArgument(const std::string& value) {
    std::string quoted = "'";
    for (const char ch : value) {
        if (ch == '\'') quoted += "'\\''";
        else quoted.push_back(ch);
    }
    quoted.push_back('\'');
    return quoted;
}
#endif

inline int runChildProcess(const std::filesystem::path& executable,
                           const std::vector<std::string>& arguments) {
#ifdef _WIN32
    const auto absoluteExecutable = std::filesystem::absolute(executable);
    std::wstring command = quoteWindowsArgument(absoluteExecutable.wstring());
    for (const auto& argument : arguments) {
        command.push_back(L' ');
        command += quoteWindowsArgument(std::filesystem::path(argument).wstring());
    }
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(absoluteExecutable.c_str(), mutableCommand.data(),
                        nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT,
                        nullptr, nullptr, &startup, &process)) {
        return -static_cast<int>(GetLastError());
    }
    CloseHandle(process.hThread);
    const DWORD waitResult = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = static_cast<DWORD>(-1);
    const BOOL readExitCode = waitResult == WAIT_OBJECT_0 &&
                              GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    return readExitCode ? static_cast<int>(exitCode) : -1;
#else
    std::string command = quotePosixArgument(std::filesystem::absolute(executable).string());
    for (const auto& argument : arguments) {
        command.push_back(' ');
        command += quotePosixArgument(argument);
    }
    const int status = std::system(command.c_str());
    if (status == -1) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return status;
#endif
}

} // namespace nexa_test
