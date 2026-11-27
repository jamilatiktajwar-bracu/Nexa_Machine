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
bool NexaShell::canTraverse(const std::string& path) const {
    std::string current="/";
    if(!security_.can(current,'x')) return false;
    std::size_t pos=1;
    while(pos<path.size()) {
        const auto end=path.find('/',pos);
        if(end==std::string::npos) break;
        const auto part=path.substr(pos,end-pos);
        if(!part.empty()) {
            current=(current=="/"?"/"+part:current+"/"+part);
            if(!security_.can(current,'x')) return false;
        }
        pos=end+1;
    }
    return true;
}
bool NexaShell::canCreateIn(const std::string& path) const {
    const auto slash=path.find_last_of('/');
    const std::string parent=slash==0?"/":path.substr(0,slash);
    return canTraverse(path)&&security_.can(parent,'w')&&security_.can(parent,'x');
}
std::string NexaShell::execute(const std::string& line) {
    std::istringstream input(line); std::string command; input>>command;
    if(command.empty()) return {};
    const auto now=std::chrono::steady_clock::now();
    if(security_.loggedIn()&&command!="login"&&now-lastActivity_>sessionTimeout_) {
        security_.logout(); currentDirectory_="/"; return "session expired; login first";
    }
    lastActivity_=now;
    try {
        if(command=="help") return "help | whoami | logout | useradd USER PASSWORD | login USER PASSWORD | passwd [USER] PASSWORD | unlock USER | groupadd GROUP | usermod -g GROUP USER | usermod -aG GROUP USER | usermod -rG GROUP USER | groups [USER] | chgrp GROUP PATH | chmod MODE PATH | chown USER PATH | pwd | cd DIR | ls [DIR] | cat PATH | touch PATH | mkdir PATH | write PATH TEXT | rm PATH | run PATH | ps | nice PID PRIORITY | limit PID INSTRUCTIONS | aging TICKS | quantum INSTRUCTIONS | timer TICKS | sched | syncstat | memstat | swapfile HOST_PATH | signal PID TERM|KILL|STOP|CONT|USR1|USR2 | kill PID | reap PID | savefs HOST_PATH | loadfs HOST_PATH";
        if(command=="whoami") return security_.loggedIn()?security_.currentUser():"(not logged in)";
        if(command=="logout") { security_.logout(); currentDirectory_="/"; return "session closed"; }
        if(!security_.loggedIn() && command!="login") return "session locked; login first";
        if(command=="useradd") { std::string user,password; input>>user>>password; if(user.empty()||password.empty()) return "usage: useradd USER PASSWORD (minimum 8 characters)"; if(security_.currentUser()!="root") return "permission denied"; return security_.addUser(user,password)?"created user "+user:"user exists, invalid, or password too short"; }
        if(command=="login") { std::string user,password; input>>user>>password; if(user.empty()||password.empty()) return "usage: login USER PASSWORD"; const bool ok=security_.login(user,password); if(ok) return "logged in as "+user; return security_.isLocked(user)?"account locked; contact root":"authentication failed"; }
        if(command=="passwd") {
            std::string first,second; input>>first>>second;
            if(first.empty()) return "usage: passwd [USER] PASSWORD";
            if(!second.empty()) {
                if(security_.currentUser()!="root") return "permission denied";
                return security_.resetPassword(first,second)?"password reset":"unknown user or password too short";
            }
            return security_.setPassword(first)?"password updated":"password must contain at least 8 characters";
        }
        if(command=="unlock") { std::string user; input>>user; if(user.empty()) return "usage: unlock USER"; if(security_.currentUser()!="root") return "permission denied"; return security_.unlockUser(user)?"account unlocked":"unknown user"; }
        if(command=="groupadd") { std::string group; input>>group; if(group.empty()) return "usage: groupadd GROUP"; if(security_.currentUser()!="root") return "permission denied"; return security_.addGroup(group)?"created group "+group:"group exists or invalid"; }
        if(command=="usermod") {
            std::string option,group,user; input>>option>>group>>user;
            if(group.empty()||user.empty()) return "usage: usermod -g GROUP USER | usermod -aG GROUP USER | usermod -rG GROUP USER";
            if(security_.currentUser()!="root") return "permission denied";
            if(option=="-g") return security_.setPrimaryGroup(user,group)?"primary group updated":"unknown user or group";
            if(option=="-aG") return security_.addSupplementaryGroup(user,group)?"supplementary group added":"unknown user/group or membership exists";
            if(option=="-rG") return security_.removeSupplementaryGroup(user,group)?"supplementary group removed":"unknown user or membership";
            return "usage: usermod -g|-aG|-rG GROUP USER";
        }
        if(command=="groups") {
            std::string user; input>>user; if(user.empty()) user=security_.currentUser();
            if(!security_.hasUser(user)) return "unknown user";
            std::string out; for(const auto& group:security_.memberships(user)) { if(!out.empty()) out+=" "; out+=group; } return out;
        }
        if(command=="chmod") {
            std::string mode,path; input>>mode>>path; if(mode.empty()||path.empty()) return "usage: chmod MODE PATH";
            std::size_t used=0; auto parsed=std::stoul(mode,&used,8);
            if(used!=mode.size()||parsed>0777) return "invalid mode (use octal, e.g. 755)";
            path=resolvePath(path); security_.chmod(path,static_cast<std::uint16_t>(parsed)); return "mode updated";
        }
        if(command=="chown") { std::string user,path; input>>user>>path; if(user.empty()||path.empty()) return "usage: chown USER PATH"; security_.chown(resolvePath(path),user); return "owner updated"; }
        if(command=="chgrp") { std::string group,path; input>>group>>path; if(group.empty()||path.empty()) return "usage: chgrp GROUP PATH"; security_.chgrp(resolvePath(path),group); return "group updated"; }
        if(command=="pwd") return currentDirectory_;
        if(command=="cd") {
            std::string path; input>>path; if(path.empty()) path="/";
            if(path=="..") { if(!security_.can(currentDirectory_,'x')) return "permission denied"; if(currentDirectory_!="/") { auto pos=currentDirectory_.find_last_of('/'); currentDirectory_=pos==0?"/":currentDirectory_.substr(0,pos); } return currentDirectory_; }
            const auto target=resolvePath(path); if(!security_.can(target,'x')||!canTraverse(target)) return "permission denied"; currentDirectory_=target; (void)kernel_.filesystem().list(currentDirectory_); return currentDirectory_;
        }
        if(command=="savefs"||command=="loadfs") {
            std::string hostPath; input>>hostPath; if(hostPath.empty()) return "usage: "+command+" HOST_PATH";
            constexpr const char* securityPath="/.__nexa_security_v1";
            if(command=="savefs") {
                auto metadata=security_.serialize();
                if(kernel_.filesystem().exists(securityPath)) kernel_.filesystem().writeFile(securityPath,metadata);
                else kernel_.filesystem().createFile(securityPath,metadata);
                kernel_.filesystem().saveImage(hostPath);
            } else {
                kernel_.filesystem().loadImage(hostPath);
                if(kernel_.filesystem().exists(securityPath)) security_.deserialize(kernel_.filesystem().readFile(securityPath));
                else security_=NexaSecurity{};
            }
            return command=="savefs"?"filesystem image saved":"filesystem image loaded";
        }
        if(command=="ls") { std::string path; input>>path; path=resolvePath(path); if(!canTraverse(path)||!security_.can(path,'r')||!security_.can(path,'x')) return "permission denied"; std::string out; for(const auto& item:kernel_.filesystem().list(path)) out+=item+"\n"; return out; }
        if(command=="cat") { std::string path; input>>path; if(path.empty()) return "usage: cat PATH"; path=resolvePath(path); if(!canTraverse(path)||!security_.can(path,'r')) return "permission denied"; auto bytes=kernel_.filesystem().readFile(path); return std::string(bytes.begin(),bytes.end()); }
        if(command=="touch") { std::string path; input>>path; if(path.empty()) return "usage: touch PATH"; path=resolvePath(path); if(!canCreateIn(path)) return "permission denied"; if(kernel_.filesystem().exists(path)) return "already exists"; kernel_.filesystem().createFile(path); security_.registerPath(path,0644); return "created "+path; }
        if(command=="mkdir") { std::string path; input>>path; if(path.empty()) return "usage: mkdir PATH"; path=resolvePath(path); if(!canCreateIn(path)) return "permission denied"; kernel_.filesystem().createDirectory(path); security_.registerPath(path,0755); return "created directory "+path; }
        if(command=="write") {
            std::string path; input>>path; std::string data; std::getline(input,data);
            if(path.empty()||data.empty()) return "usage: write PATH TEXT"; if(data.front()==' ') data.erase(data.begin());
            path=resolvePath(path); if(kernel_.filesystem().exists(path)&&(!canTraverse(path)||!security_.can(path,'w'))) return "permission denied"; if(!kernel_.filesystem().exists(path)&&!canCreateIn(path)) return "permission denied";
            std::vector<std::uint8_t> bytes(data.begin(),data.end());
            if(kernel_.filesystem().exists(path)) kernel_.filesystem().writeFile(path,bytes);
            else { kernel_.filesystem().createFile(path,bytes); security_.registerPath(path,0644); }
            return "wrote "+std::to_string(bytes.size())+" bytes";
        }
        if(command=="rm") { std::string path; input>>path; if(path.empty()) return "usage: rm PATH"; path=resolvePath(path); if(!canCreateIn(path)) return "permission denied"; kernel_.filesystem().removeFile(path); security_.forgetPath(path); return "removed "+path; }
        if(command=="run") {
            std::string path; input>>path; if(path.empty()) return "usage: run PATH"; path=resolvePath(path);
            if(!canTraverse(path)||!security_.can(path,'x')) return "permission denied";
            auto bytes=kernel_.filesystem().readFile(path); std::string source(bytes.begin(),bytes.end());
            Executable image{0,parseAssembly(source)}; auto pid=kernel_.spawn(path,image); kernel_.run(); auto out=kernel_.takeConsoleOutput();
            auto processes=kernel_.processes(); auto found=std::find_if(processes.begin(),processes.end(),[pid](const auto& p){return p.pid==pid;});
            if(found!=processes.end()&&found->state==ProcessState::Faulted) out+="\nprocess fault: "+found->faultReason; return out;
        }
        if(command=="limit") {
            std::string pidText,budgetText; input>>pidText>>budgetText;
            if(pidText.empty()||budgetText.empty()) return "usage: limit PID INSTRUCTIONS (0 disables)";
            if(security_.currentUser()!="root") return "permission denied";
            std::size_t pidUsed=0,budgetUsed=0;
            const auto pidValue=std::stoull(pidText,&pidUsed);
            const auto budget=std::stoull(budgetText,&budgetUsed);
            if(pidUsed!=pidText.size()||budgetUsed!=budgetText.size()||pidValue>0xFFFFFFFFu)
                return "invalid PID or instruction budget";
            kernel_.setInstructionBudget(static_cast<std::uint32_t>(pidValue),budget);
            return "instruction budget updated";
        }
        if(command=="nice") {
            std::string pidText,priorityText; input>>pidText>>priorityText;
            if(pidText.empty()||priorityText.empty()) return "usage: nice PID PRIORITY";
            if(security_.currentUser()!="root") return "permission denied";
            std::size_t pidUsed=0,priorityUsed=0;
            const auto pidValue=std::stoull(pidText,&pidUsed);
            const auto priorityValue=std::stoul(priorityText,&priorityUsed);
            if(pidUsed!=pidText.size()||priorityUsed!=priorityText.size()||pidValue>0xFFFFFFFFu||priorityValue>7)
                return "invalid PID or priority (0-7)";
            kernel_.setPriority(static_cast<std::uint32_t>(pidValue),static_cast<std::uint8_t>(priorityValue));
            return "priority updated";
        }
        if(command=="signal") {
            std::string pidText,signalText; input>>pidText>>signalText;
            if(pidText.empty()||signalText.empty()) return "usage: signal PID TERM|KILL|STOP|CONT|USR1|USR2";
            if(security_.currentUser()!="root") return "permission denied";
            std::size_t used=0; const auto pidValue=std::stoull(pidText,&used);
            if(used!=pidText.size()||pidValue>0xFFFFFFFFu) return "invalid PID";
            ProcessSignal signal;
            if(signalText=="TERM") signal=ProcessSignal::Terminate;
            else if(signalText=="KILL") signal=ProcessSignal::Kill;
            else if(signalText=="STOP") signal=ProcessSignal::Stop;
            else if(signalText=="CONT") signal=ProcessSignal::Continue;
            else if(signalText=="USR1") signal=ProcessSignal::User1;
            else if(signalText=="USR2") signal=ProcessSignal::User2;
            else return "invalid signal (TERM, KILL, STOP, CONT, USR1, USR2)";
            return kernel_.sendSignal(static_cast<std::uint32_t>(pidValue),signal)?"signal delivered":"signal not accepted";
        }
        if(command=="kill") {
            std::string pidText; input>>pidText;
            if(pidText.empty()) return "usage: kill PID";
            if(security_.currentUser()!="root") return "permission denied";
            std::size_t used=0; const auto pidValue=std::stoull(pidText,&used);
            if(used!=pidText.size()||pidValue>0xFFFFFFFFu) return "invalid PID";
            return kernel_.terminateProcess(static_cast<std::uint32_t>(pidValue))?"process terminated":"unknown or already stopped process";
        }
        if(command=="reap") {
            std::string pidText; input>>pidText;
            if(pidText.empty()) return "usage: reap PID";
            if(security_.currentUser()!="root") return "permission denied";
            std::size_t used=0; const auto pidValue=std::stoull(pidText,&used);
            if(used!=pidText.size()||pidValue>0xFFFFFFFFu) return "invalid PID";
            return kernel_.reapProcess(static_cast<std::uint32_t>(pidValue))?"process reaped":"unknown or active process";
        }
        if(command=="memstat") {
            const auto& mm=kernel_.memoryManager();
            return "frames_used="+std::to_string(mm.residentPages())+
                " frames_total="+std::to_string(mm.totalFrames())+
                " frames_free="+std::to_string(mm.freeFrames())+
                " swapped_pages="+std::to_string(mm.swappedPages())+
                " swap_ins="+std::to_string(mm.swapIns())+
                " swap_outs="+std::to_string(mm.swapOuts())+
                " page_faults="+std::to_string(mm.pageFaults())+
                " tlb_hits="+std::to_string(mm.tlbHits())+
                " tlb_misses="+std::to_string(mm.tlbMisses())+
                " swap_backend="+(mm.swapFilePath().empty()?"memory":"disk");
        }
        if(command=="swapfile") {
            std::string hostPath; input>>hostPath;
            if(hostPath.empty()) return "usage: swapfile HOST_PATH";
            if(security_.currentUser()!="root") return "permission denied";
            kernel_.memoryManager().setSwapFile(hostPath);
            return "disk swap configured";
        }
        if(command=="quantum") {
            std::string valueText; input>>valueText;
            if(valueText.empty()) return "usage: quantum INSTRUCTIONS";
            if(security_.currentUser()!="root") return "permission denied";
            std::size_t used=0; const auto value=std::stoull(valueText,&used);
            if(used!=valueText.size()||value==0||value>1000000) return "invalid quantum (1-1000000)";
            kernel_.setTimeQuantum(static_cast<std::size_t>(value));
            return "time quantum updated";
        }
        if(command=="timer") {
            std::string valueText; input>>valueText;
            if(valueText.empty()) return "usage: timer TICKS (0 disables)";
            if(security_.currentUser()!="root") return "permission denied";
            std::size_t used=0; const auto value=std::stoull(valueText,&used);
            if(used!=valueText.size()) return "invalid timer interval";
            kernel_.setTimerInterval(value);
            return "timer interval updated";
        }
        if(command=="aging") {
            std::string thresholdText; input>>thresholdText;
            if(thresholdText.empty()) return "usage: aging TICKS (0 disables)";
            if(security_.currentUser()!="root") return "permission denied";
            std::size_t used=0; const auto threshold=std::stoull(thresholdText,&used);
            if(used!=thresholdText.size()) return "invalid aging threshold";
            kernel_.setPriorityAgingThreshold(threshold);
            return "priority aging updated";
        }
        if(command=="syncstat") return kernel_.syncDiagnostics();\n        if(command=="sched") {
            return "policy=fixed-priority-round-robin quantum="+std::to_string(kernel_.timeQuantum())+
                " ticks="+std::to_string(kernel_.systemTicks())+
                " timer_interval="+std::to_string(kernel_.timerInterval())+
                " timer_irqs="+std::to_string(kernel_.timerInterrupts())+
                " pending_irqs="+std::to_string(kernel_.pendingTimerInterrupts())+
                " aging_threshold="+std::to_string(kernel_.priorityAgingThreshold())+
                " deadlock="+(kernel_.hasDeadlock()?"yes":"no");
        }
        if(command=="ps") {
            std::string out;
            for(const auto& p:kernel_.processes()) {
                const char* state=p.state==ProcessState::Ready?"ready":p.state==ProcessState::Running?"running":p.state==ProcessState::Blocked?"blocked":p.state==ProcessState::Stopped?"stopped":p.state==ProcessState::Halted?"halted":"faulted";
                out+=std::to_string(p.pid)+" "+p.name+" "+state+" priority="+std::to_string(p.priority)+
                    " effective_priority="+std::to_string(p.effectivePriority)+" ticks="+std::to_string(p.ticks)+
                    " wait_ticks="+std::to_string(p.readyWaitTicks)+" budget="+std::to_string(p.instructionBudget)+" inbox="+std::to_string(p.pendingMessages)+" signals="+std::to_string(p.pendingSignals)+
                    " exit_code="+(p.exitCode?std::to_string(*p.exitCode):"-")+
                    " reason="+(p.terminationReason.empty()?"-":p.terminationReason);
                if(!p.faultReason.empty()) out+=" fault="+p.faultReason;
                out+="\n";
            }
            return out;
        }
        return "unknown command: "+command+" (type help)";
    } catch(const std::exception& e) { return std::string("error: ")+e.what(); }
}
}
