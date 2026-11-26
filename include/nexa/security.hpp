#pragma once
#include <cstdint>
#include <map>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <array>
#include <set>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace nexa {
struct FileOwner { std::string user{"root"}; std::string group{"root"}; std::uint16_t mode{0644}; };

class NexaSecurity {
    struct UserRecord {
        std::string group;
        std::array<unsigned char,16> salt{};
        std::array<unsigned char,32> digest{};
        bool passwordSet{false};
        unsigned failedAttempts{0};
        bool locked{false};
    };
    static constexpr int iterations_ = 120000;
    static constexpr unsigned maxFailedAttempts_ = 5;
public:
    NexaSecurity() { users_["root"]={"root",{}, {},false}; }

    bool addUser(const std::string& name, const std::string& password) {
        if(name.empty() || name.find('/')!=std::string::npos || name.find('"')!=std::string::npos || users_.count(name) || password.size()<8) return false;
        UserRecord record; record.group=name; hashPassword(record,password);
        groups_.insert(record.group); users_.emplace(name,record); return true;
    }
    bool hasUser(const std::string& name) const { return users_.count(name)!=0; }
    bool isLocked(const std::string& name) const { auto it=users_.find(name); return it!=users_.end()&&it->second.locked; }
    bool authenticate(const std::string& name, const std::string& password) const {
        const auto it=users_.find(name);
        if(it==users_.end() || !it->second.passwordSet || it->second.locked) return false;
        std::array<unsigned char,32> candidate{};
        if(PKCS5_PBKDF2_HMAC(password.data(),static_cast<int>(password.size()),it->second.salt.data(),
            static_cast<int>(it->second.salt.size()),iterations_,EVP_sha256(),static_cast<int>(candidate.size()),candidate.data())!=1)
            throw std::runtime_error("password verification failed");
        unsigned char diff=0;
        for(std::size_t i=0;i<candidate.size();++i) diff|=candidate[i]^it->second.digest[i];
        return diff==0;
    }
    bool login(const std::string& name, const std::string& password) {
        auto it=users_.find(name);
        if(it==users_.end() || it->second.locked || !it->second.passwordSet) return false;
        if(!authenticate(name,password)) { if(++it->second.failedAttempts>=maxFailedAttempts_) it->second.locked=true; return false; }
        it->second.failedAttempts=0;
        current_=name;
        return true;
    }
    bool setPassword(const std::string& password) {
        if(password.size()<8) return false;
        auto& record=users_.at(current_); hashPassword(record,password);
        record.failedAttempts=0; record.locked=false; return true;
    }
    bool resetPassword(const std::string& user,const std::string& password) {
        auto it=users_.find(user); if(it==users_.end()||password.size()<8) return false;
        hashPassword(it->second,password); it->second.failedAttempts=0; it->second.locked=false; return true;
    }
    bool unlockUser(const std::string& user) {
        auto it=users_.find(user); if(it==users_.end()) return false;
        it->second.failedAttempts=0; it->second.locked=false; return true;
    }
    bool addGroup(const std::string& group) {
        if(group.empty()||group.find('/')!=std::string::npos||group.find('"')!=std::string::npos) return false;
        return groups_.insert(group).second;
    }
    bool setPrimaryGroup(const std::string& user,const std::string& group) {
        auto it=users_.find(user); if(it==users_.end()||!groups_.count(group)) return false;
        it->second.group=group; return true;
    }
    bool hasGroup(const std::string& group) const { return groups_.count(group)!=0; }
    const std::string& currentUser() const noexcept { return current_; }

    [[nodiscard]] std::vector<std::uint8_t> serialize() const {
        std::ostringstream out;
        out << "NEXASEC3\n";
        for(const auto& group:groups_) out << "G " << std::quoted(group) << "\n";
        for(const auto& [name,record]:users_) {
            out << "U " << std::quoted(name) << " " << std::quoted(record.group) << " " << record.passwordSet << " " << record.failedAttempts << " " << record.locked << " ";
            for(auto b:record.salt) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(b);
            out << " ";
            for(auto b:record.digest) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(b);
            out << std::dec << "\n";
        }
        for(const auto& [path,meta]:owners_) out << "F " << std::quoted(path) << " " << std::quoted(meta.user) << " " << std::quoted(meta.group) << " " << meta.mode << "\n";
        const auto data=out.str(); return {data.begin(),data.end()};
    }

    void deserialize(const std::vector<std::uint8_t>& bytes) {
        const std::string data(bytes.begin(),bytes.end()); std::istringstream in(data);
        std::string header; if(!std::getline(in,header)||(header!="NEXASEC1"&&header!="NEXASEC2"&&header!="NEXASEC3")) throw std::invalid_argument("invalid security metadata");
        const bool version2=header=="NEXASEC2"||header=="NEXASEC3";
        const bool version3=header=="NEXASEC3";
        std::map<std::string,UserRecord> users{{"root",UserRecord{"root",{}, {},false}}};
        auto owners=std::map<std::string,FileOwner>{};
        std::set<std::string> groups = version3 ? std::set<std::string>{} : std::set<std::string>{"root"};
        bool rootSeen=false;
        std::string line;
        while(std::getline(in,line)) {
            if(line.empty()) continue;
            std::istringstream row(line); char kind{}; std::string a,b,c; row>>kind;
            if(kind=='G') { row>>std::quoted(a); if(!row||a.empty()||!groups.insert(a).second) throw std::invalid_argument("invalid security group"); std::string extra; if(row>>extra) throw std::invalid_argument("extra security metadata fields"); continue; }
            row>>std::quoted(a)>>std::quoted(b);
            if(!row) throw std::invalid_argument("malformed security metadata");
            if(kind=='U') {
                UserRecord rec; rec.group=b;
                if(version2) {
                    unsigned set{}; std::string saltHex,digestHex; row>>set;
                    if(version3) { unsigned locked{}; row>>rec.failedAttempts>>locked; if(!row||rec.failedAttempts>maxFailedAttempts_||locked>1) throw std::invalid_argument("invalid account lockout metadata"); rec.locked=locked!=0; }
                    row>>saltHex>>digestHex;
                    if(!row||set>1||saltHex.size()!=32||digestHex.size()!=64) throw std::invalid_argument("invalid password metadata");
                    rec.passwordSet=set!=0; decodeHex(saltHex,rec.salt.data(),rec.salt.size()); decodeHex(digestHex,rec.digest.data(),rec.digest.size());
                }
                if(!version3) groups.insert(rec.group);
                if(a=="root") {
                    if(rootSeen) throw std::invalid_argument("duplicate security user");
                    users["root"]=rec;
                    rootSeen=true;
                } else if(!users.emplace(a,rec).second) {
                    throw std::invalid_argument("duplicate security user");
                }
            } else if(kind=='F') {
                unsigned mode{}; row>>std::quoted(c)>>mode;
                if(!row||mode>0777||a.empty()||users.count(b)==0) throw std::invalid_argument("invalid file metadata");
                if(!version3) groups.insert(c);
                if(!owners.emplace(a,FileOwner{b,c,static_cast<std::uint16_t>(mode)}).second) throw std::invalid_argument("duplicate file metadata");
            } else throw std::invalid_argument("unknown security metadata record");
            std::string extra; if(row>>extra) throw std::invalid_argument("extra security metadata fields");
        }
        for(const auto& [name,record]:users) if(!groups.count(record.group)) throw std::invalid_argument("unknown user group");
        for(const auto& [path,meta]:owners) if(!groups.count(meta.group)) throw std::invalid_argument("unknown file group");
        groups_=std::move(groups); users_=std::move(users); owners_=std::move(owners); current_="root";
    }
    void registerPath(const std::string& path,std::uint16_t mode) {
        if(!owners_.count(path)) owners_[path]={current_,users_.at(current_).group,mode};
    }
    const FileOwner& owner(const std::string& path) const {
        auto it=owners_.find(path); if(it==owners_.end()) throw std::out_of_range("file metadata not found"); return it->second;
    }
    void chmod(const std::string& path,std::uint16_t mode) { requireOwner(path); owners_.at(path).mode=mode; }
    void chown(const std::string& path,const std::string& user) {
        requireOwner(path); if(!hasUser(user)) throw std::invalid_argument("unknown user");
        owners_.at(path).user=user; owners_.at(path).group=users_.at(user).group;
    }
    void chgrp(const std::string& path,const std::string& group) {
        requireOwner(path); if(!groups_.count(group)) throw std::invalid_argument("unknown group");
        owners_.at(path).group=group;
    }
    void forgetPath(const std::string& path) { owners_.erase(path); }
    bool can(const std::string& path,char permission) const {
        if(current_=="root") return true;
        auto it=owners_.find(path); if(it==owners_.end()) return false;
        const auto& m=it->second; unsigned shift=current_==m.user?6:(users_.at(current_).group==m.group?3:0);
        unsigned bit=permission=='r'?4:permission=='w'?2:permission=='x'?1:0;
        return bit && ((m.mode>>shift)&bit)!=0;
    }
private:
    std::map<std::string,UserRecord> users_;
    std::set<std::string> groups_{"root"};
    std::map<std::string,FileOwner> owners_;
    std::string current_{"root"};

    static void decodeHex(const std::string& text,unsigned char* output,std::size_t size) {
        auto digit=[](char c)->int { if(c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return c-'a'+10; if(c>='A'&&c<='F') return c-'A'+10; return -1; };
        for(std::size_t i=0;i<size;++i) { int hi=digit(text[2*i]),lo=digit(text[2*i+1]); if(hi<0||lo<0) throw std::invalid_argument("invalid hex password metadata"); output[i]=static_cast<unsigned char>((hi<<4)|lo); }
    }
    static void hashPassword(UserRecord& record,const std::string& password) {
        if(RAND_bytes(record.salt.data(),static_cast<int>(record.salt.size()))!=1) throw std::runtime_error("secure random generator failed");
        if(PKCS5_PBKDF2_HMAC(password.data(),static_cast<int>(password.size()),record.salt.data(),
            static_cast<int>(record.salt.size()),iterations_,EVP_sha256(),static_cast<int>(record.digest.size()),record.digest.data())!=1)
            throw std::runtime_error("password hashing failed");
        record.passwordSet=true;
    }
    void requireOwner(const std::string& path) const {
        auto it=owners_.find(path); if(it==owners_.end()) throw std::out_of_range("file metadata not found");
        if(current_!="root"&&it->second.user!=current_) throw std::runtime_error("permission denied");
    }
};
}
