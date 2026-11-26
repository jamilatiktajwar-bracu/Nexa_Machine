#include "nexa/shell.hpp"
#include "nexa/assembler.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>
namespace nexa {
std::string NexaShell::resolvePath(const std::string& path) const {
    if(path.empty()) return currentDirectory_;
    if(path.front()=='/') return path;
    return currentDirectory_=="/"?"/"+path:currentDirectory_+"/"+path;
}
std::string NexaShell::execute(const std::string& line) {
    std::istringstream input(line);
    std::string command;
    input >> command;
    if(command.empty()) return {};
    try {
        if(command=="help")
            return "help | pwd | cd DIR | ls [DIR] | cat PATH | touch PATH | mkdir PATH | write PATH TEXT | rm PATH | run PATH | ps | savefs HOST_PATH | loadfs HOST_PATH";
        if(command=="pwd") return currentDirectory_;
        if(command=="cd") {
            std::string path; input>>path; if(path.empty()) path="/";
            if(path=="..") {
                if(currentDirectory_!="/") { auto pos=currentDirectory_.find_last_of('/'); currentDirectory_=pos==0?"/":currentDirectory_.substr(0,pos); }
                return currentDirectory_;
            }
            const auto target=resolvePath(path);
            (void)kernel_.filesystem().list(target);
            currentDirectory_=target;
            return currentDirectory_;
        }
        if(command=="savefs" || command=="loadfs") {
            std::string hostPath; input>>hostPath;
            if(hostPath.empty()) return "usage: "+command+" HOST_PATH";
            if(command=="savefs") kernel_.filesystem().saveImage(hostPath);
            else kernel_.filesystem().loadImage(hostPath);
            return command=="savefs"?"filesystem image saved":"filesystem image loaded";
        }
        if(command=="ls") {
            std::string path; input>>path; path=resolvePath(path);
            std::string result;
            for(const auto& item:kernel_.filesystem().list(path)) result+=item+"\n";
            return result;
        }
        if(command=="cat") {
            std::string path; input>>path;
            if(path.empty()) return "usage: cat PATH";
            const auto bytes=kernel_.filesystem().readFile(resolvePath(path));
            return std::string(bytes.begin(),bytes.end());
        }
        if(command=="touch") {
            std::string path; input>>path;
            if(path.empty()) return "usage: touch PATH";
            path=resolvePath(path);
            if(kernel_.filesystem().exists(path)) return "already exists";
            kernel_.filesystem().createFile(path);
            return "created "+path;
        }
        if(command=="mkdir") {
            std::string path; input>>path;
            if(path.empty()) return "usage: mkdir PATH";
            path=resolvePath(path);
            kernel_.filesystem().createDirectory(path);
            return "created directory "+path;
        }
        if(command=="write") {
            std::string path; input>>path;
            std::string content; std::getline(input,content);
            if(path.empty()||content.empty()) return "usage: write PATH TEXT";
            if(content.front()==' ') content.erase(content.begin());
            path=resolvePath(path);
            std::vector<std::uint8_t> bytes(content.begin(),content.end());
            if(kernel_.filesystem().exists(path)) kernel_.filesystem().writeFile(path,bytes);
            else kernel_.filesystem().createFile(path,bytes);
            return "wrote "+std::to_string(bytes.size())+" bytes";
        }
        if(command=="rm") {
            std::string path; input>>path;
            if(path.empty()) return "usage: rm PATH";
            path=resolvePath(path);
            kernel_.filesystem().removeFile(path);
            return "removed "+path;
        }
        if(command=="run") {
            std::string path; input>>path;
            if(path.empty()) return "usage: run PATH";
            path=resolvePath(path);
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
