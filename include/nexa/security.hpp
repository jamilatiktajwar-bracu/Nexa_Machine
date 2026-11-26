#pragma once
#include <cstdint>
#include <map>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
namespace nexa {
struct FileOwner { std::string user{"root"}; std::string group{"root"}; std::uint16_t mode{0644}; };
class NexaSecurity {
public:
    NexaSecurity() { users_["root"]="root"; }
    bool addUser(const std::string& name) {
        if(name.empty() || name.find('/')!=std::string::npos || users_.count(name)) return false;
        users_[name]=name; return true;
    }
    bool hasUser(const std::string& name) const { return users_.count(name)!=0; }
    [[nodiscard]] std::vector<std::uint8_t> serialize() const {
        std::ostringstream out;
        out << "NEXASEC1\\n";
        for (const auto& [name, group] : users_) out << "U " << std::quoted(name) << " " << std::quoted(group) << "\\n";
        for (const auto& [path, meta] : owners_) out << "F " << std::quoted(path) << " " << std::quoted(meta.user) << " " << std::quoted(meta.group) << " " << meta.mode << "\\n";
        const auto data=out.str(); return {data.begin(),data.end()};
    }
    void deserialize(const std::vector<std::uint8_t>& bytes) {
        const std::string data(bytes.begin(),bytes.end()); std::istringstream in(data);
        std::string header; if(!std::getline(in,header)||header!="NEXASEC1") throw std::invalid_argument("invalid security metadata");
        auto users=std::map<std::string,std::string>{{"root","root"}};
        auto owners=std::map<std::string,FileOwner>{}; std::string line;
        while(std::getline(in,line)) {
            if(line.empty()) continue;
            std::istringstream row(line); char kind{}; std::string a,b,c; row>>kind>>std::quoted(a)>>std::quoted(b);
            if(!row) throw std::invalid_argument("malformed security metadata");
            if(kind=='U') { if(!users.emplace(a,b).second) throw std::invalid_argument("duplicate security user"); }
            else if(kind=='F') {
                unsigned mode{}; row>>std::quoted(c)>>mode;
                if(!row||mode>0777||a.empty()||users.count(b)==0) throw std::invalid_argument("invalid file metadata");
                if(!owners.emplace(a,FileOwner{b,c,static_cast<std::uint16_t>(mode)}).second) throw std::invalid_argument("duplicate file metadata");
            } else throw std::invalid_argument("unknown security metadata record");
            std::string extra; if(row>>extra) throw std::invalid_argument("extra security metadata fields");
        }
        users_=std::move(users); owners_=std::move(owners); current_="root";
    }
    bool login(const std::string& name) { if(!hasUser(name)) return false; current_=name; return true; }
    const std::string& currentUser() const noexcept { return current_; }
    void registerPath(const std::string& path, std::uint16_t mode) {
        if(!owners_.count(path)) owners_[path]={current_,users_.at(current_),mode};
    }
    const FileOwner& owner(const std::string& path) const {
        auto it=owners_.find(path); if(it==owners_.end()) throw std::out_of_range("file metadata not found");
        return it->second;
    }
    void chmod(const std::string& path, std::uint16_t mode) {
        requireOwner(path); owners_.at(path).mode=mode;
    }
    void chown(const std::string& path, const std::string& user) {
        requireOwner(path); if(!hasUser(user)) throw std::invalid_argument("unknown user");
        owners_.at(path).user=user; owners_.at(path).group=users_.at(user);
    }
    void forgetPath(const std::string& path) { owners_.erase(path); }
    bool can(const std::string& path, char permission) const {
        if(current_=="root") return true;
        auto it=owners_.find(path); if(it==owners_.end()) return false;
        const auto& m=it->second;
        unsigned shift=current_==m.user?6:(users_.at(current_)==m.group?3:0);
        unsigned bit=permission=='r'?4:permission=='w'?2:permission=='x'?1:0;
        return bit && ((m.mode>>shift)&bit)!=0;
    }
private:
    std::map<std::string,std::string> users_;
    std::map<std::string,FileOwner> owners_;
    std::string current_{"root"};
    void requireOwner(const std::string& path) const {
        auto it=owners_.find(path);
        if(it==owners_.end()) throw std::out_of_range("file metadata not found");
        if(current_!="root" && it->second.user!=current_) throw std::runtime_error("permission denied");
    }
};
}
