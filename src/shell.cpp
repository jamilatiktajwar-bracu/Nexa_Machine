#include "nexa/shell.hpp"
#include "nexa/assembler.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>
namespace nexa {
std::string NexaShell::execute(const std::string& line) {
    std::istringstream input(line);
    std::string command;
    input >> command;
    if(command.empty()) return {};
    try {
        if(command=="help")
            return "help | ls [DIR] | cat PATH | touch PATH | mkdir PATH | write PATH TEXT | rm PATH | run PATH | ps";
        if(command=="ls") {
            std::string path="/"; input>>path;
            std::string result;
            for(const auto& item:kernel_.filesystem().list(path)) result+=item+"\n";
            return result;
        }
        if(command=="cat") {
            std::string path; input>>path;
            if(path.empty()) return "usage: cat PATH";
            const auto bytes=kernel_.filesystem().readFile(path);
            return std::string(bytes.begin(),bytes.end());
        }
        if(command=="touch") {
            std::string path; input>>path;
            if(path.empty()) return "usage: touch PATH";
            if(kernel_.filesystem().exists(path)) return "already exists";
            kernel_.filesystem().createFile(path);
            return "created "+path;
        }
        if(command=="mkdir") {
            std::string path; input>>path;
            if(path.empty()) return "usage: mkdir PATH";
            kernel_.filesystem().createDirectory(path);
            return "created directory "+path;
        }
        if(command=="write") {
            std::string path; input>>path;
            std::string content; std::getline(input,content);
            if(path.empty()||content.empty()) return "usage: write PATH TEXT";
            if(content.front()==' ') content.erase(content.begin());
            std::vector<std::uint8_t> bytes(content.begin(),content.end());
            if(kernel_.filesystem().exists(path)) kernel_.filesystem().writeFile(path,bytes);
            else kernel_.filesystem().createFile(path,bytes);
            return "wrote "+std::to_string(bytes.size())+" bytes";
        }
        if(command=="rm") {
            std::string path; input>>path;
            if(path.empty()) return "usage: rm PATH";
            kernel_.filesystem().removeFile(path);
            return "removed "+path;
        }
        if(command=="run") {
            std::string path; input>>path;
            if(path.empty()) return "usage: run PATH";
            const auto bytes=kernel_.filesystem().readFile(path);
            const std::string source(bytes.begin(),bytes.end());
            Executable image{0,parseAssembly(source)};
            const auto pid=kernel_.spawn(path,image);
            kernel_.run();
            auto result=kernel_.takeConsoleOutput();
            const auto processes=kernel_.processes();
            const auto found=std::find_if(processes.begin(),processes.end(),[pid](const auto& p){return p.pid==pid;});
            if(found!=processes.end()&&found->state==ProcessState::Faulted)
                result+="\nprocess fault: "+found->faultReason;
            return result;
        }
        if(command=="ps") {
            std::string result;
            for(const auto& process:kernel_.processes()) {
                const char* state=process.state==ProcessState::Ready?"ready":process.state==ProcessState::Running?"running":process.state==ProcessState::Halted?"halted":"faulted";
                result+=std::to_string(process.pid)+" "+process.name+" "+state+" ticks="+std::to_string(process.ticks)+"\n";
            }
            return result;
        }
        return "unknown command: "+command+" (type help)";
    } catch(const std::exception& error) {
        return std::string("error: ")+error.what();
    }
}
}
