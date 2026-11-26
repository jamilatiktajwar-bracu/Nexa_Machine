#include "nexa/vfs.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
namespace nexa {
namespace {
constexpr std::array<char,8> Magic={'N','E','X','A','V','F','S','1'};
constexpr std::uint32_t MaxEntries=100000;
constexpr std::uint32_t MaxPathBytes=4096;
constexpr std::uint32_t MaxFileBytes=64U*1024U*1024U;
void put32(std::ostream& out,std::uint32_t v){for(unsigned i=0;i<4;++i)out.put(static_cast<char>((v>>(i*8))&255));if(!out)throw std::runtime_error("Failed writing VFS image");}
std::uint32_t get32(std::istream& in){std::uint32_t v=0;for(unsigned i=0;i<4;++i){int c=in.get();if(c==std::char_traits<char>::eof())throw std::runtime_error("Truncated VFS image");v|=static_cast<std::uint32_t>(static_cast<unsigned char>(c))<<(i*8);}return v;}
void putString(std::ostream& out,const std::string& s){if(s.size()>MaxPathBytes)throw std::runtime_error("VFS path too long");put32(out,static_cast<std::uint32_t>(s.size()));out.write(s.data(),static_cast<std::streamsize>(s.size()));if(!out)throw std::runtime_error("Failed writing VFS path");}
std::string getString(std::istream& in){auto n=get32(in);if(n==0||n>MaxPathBytes)throw std::runtime_error("Invalid VFS path length");std::string s(n,'\\0');in.read(s.data(),static_cast<std::streamsize>(n));if(!in)throw std::runtime_error("Truncated VFS path");return s;}
}
std::string VirtualFileSystem::normalize(const std::string&p){if(p.empty())throw std::invalid_argument("Empty VFS path");std::string s=p.front()=='/'?p:"/"+p;std::string out;for(std::size_t i=0;i<s.size();){while(i<s.size()&&s[i]=='/')++i;auto j=i;while(j<s.size()&&s[j]!='/')++j;if(j==i)break;auto part=s.substr(i,j-i);if(part=="..")throw std::invalid_argument("Parent traversal is not allowed");if(part!=".")out+="/"+part;i=j;}return out.empty()?"/":out;}
void VirtualFileSystem::createDirectory(const std::string&p){auto n=normalize(p);if(std::find(directories_.begin(),directories_.end(),n)!=directories_.end())return;auto parent=n.substr(0,n.find_last_of('/'));if(parent.empty())parent="/";if(std::find(directories_.begin(),directories_.end(),parent)==directories_.end())throw std::runtime_error("Parent directory missing");if(files_.contains(n))throw std::runtime_error("A file already uses this path");directories_.push_back(n);}
void VirtualFileSystem::createFile(const std::string&p,const std::vector<std::uint8_t>&d){auto n=normalize(p);if(files_.contains(n)||std::find(directories_.begin(),directories_.end(),n)!=directories_.end())throw std::runtime_error("VFS path already exists");auto parent=n.substr(0,n.find_last_of('/'));if(parent.empty())parent="/";if(std::find(directories_.begin(),directories_.end(),parent)==directories_.end())throw std::runtime_error("Parent directory missing");files_[n]=d;}
void VirtualFileSystem::writeFile(const std::string&p,const std::vector<std::uint8_t>&d){auto n=normalize(p);if(!files_.contains(n))throw std::runtime_error("File not found");files_[n]=d;}
std::vector<std::uint8_t>VirtualFileSystem::readFile(const std::string&p)const{auto n=normalize(p);auto it=files_.find(n);if(it==files_.end())throw std::runtime_error("File not found");return it->second;}
bool VirtualFileSystem::exists(const std::string&p)const{auto n=normalize(p);return files_.contains(n)||std::find(directories_.begin(),directories_.end(),n)!=directories_.end();}
void VirtualFileSystem::removeFile(const std::string&p){auto n=normalize(p);if(!files_.erase(n))throw std::runtime_error("File not found");}
std::vector<std::string>VirtualFileSystem::list(const std::string&p)const{auto dir=normalize(p);if(std::find(directories_.begin(),directories_.end(),dir)==directories_.end())throw std::runtime_error("Directory not found");std::vector<std::string>r;auto prefix=dir=="/"?"/":dir+"/";for(auto&d:directories_)if(d!=dir&&d.rfind(prefix,0)==0&&d.find('/',prefix.size())==std::string::npos)r.push_back(d.substr(prefix.size()));for(auto&f:files_)if(f.first.rfind(prefix,0)==0&&f.first.find('/',prefix.size())==std::string::npos)r.push_back(f.first.substr(prefix.size()));std::sort(r.begin(),r.end());return r;}
void VirtualFileSystem::saveImage(const std::string& path) const {
 std::ofstream out(path,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("Cannot create VFS image: "+path);
 out.write(Magic.data(),static_cast<std::streamsize>(Magic.size()));
 if(directories_.size()>MaxEntries||files_.size()>MaxEntries||directories_.size()+files_.size()>MaxEntries)throw std::runtime_error("Too many VFS entries");
 put32(out,static_cast<std::uint32_t>(directories_.size()-1));put32(out,static_cast<std::uint32_t>(files_.size()));
 for(const auto& d:directories_)if(d!="/")putString(out,d);
 for(const auto& [name,data]:files_){putString(out,name);if(data.size()>MaxFileBytes)throw std::runtime_error("VFS file too large");put32(out,static_cast<std::uint32_t>(data.size()));out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));if(!out)throw std::runtime_error("Failed writing VFS file data");}
}
void VirtualFileSystem::loadImage(const std::string& path) {
 std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot open VFS image: "+path);
 std::array<char,8> magic{};in.read(magic.data(),static_cast<std::streamsize>(magic.size()));if(!in||magic!=Magic)throw std::runtime_error("Invalid VFS image header");
 auto dirCount=get32(in),fileCount=get32(in);if(dirCount>MaxEntries||fileCount>MaxEntries||static_cast<std::uint64_t>(dirCount)+fileCount>MaxEntries)throw std::runtime_error("VFS image entry limit exceeded");
 std::vector<std::string> dirs{"/"};std::set<std::string> dirSet{"/"};std::map<std::string,std::vector<std::uint8_t>> files;
 for(std::uint32_t i=0;i<dirCount;++i){auto name=getString(in);auto normalized=normalize(name);if(normalized!=name||normalized=="/"||!dirSet.insert(normalized).second)throw std::runtime_error("Invalid or duplicate VFS directory");auto parent=normalized.substr(0,normalized.find_last_of('/'));if(parent.empty())parent="/";if(!dirSet.contains(parent))throw std::runtime_error("VFS directory parent missing");dirs.push_back(normalized);}
 for(std::uint32_t i=0;i<fileCount;++i){auto name=getString(in);auto normalized=normalize(name);if(normalized!=name||normalized=="/"||files.contains(normalized)||dirSet.contains(normalized))throw std::runtime_error("Invalid or duplicate VFS file");auto parent=normalized.substr(0,normalized.find_last_of('/'));if(parent.empty())parent="/";if(!dirSet.contains(parent))throw std::runtime_error("VFS file parent missing");auto size=get32(in);if(size>MaxFileBytes)throw std::runtime_error("VFS file size limit exceeded");std::vector<std::uint8_t> data(size);in.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(size));if(!in)throw std::runtime_error("Truncated VFS file data");files.emplace(normalized,std::move(data));}
 char extra;if(in.get(extra))throw std::runtime_error("Unexpected trailing VFS image data");
 directories_=std::move(dirs);files_=std::move(files);
}
}
