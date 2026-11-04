#include "save_backups.h"
#include "asset_hash.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <set>

namespace iu::saves {
namespace fs = std::filesystem;
namespace {
constexpr size_t kMaxBytes = 32 * 1024 * 1024;
constexpr size_t kKeep = 10;
using Bytes = std::vector<uint8_t>;
struct Snapshot {
  std::string package, hash;
  std::map<std::string, Bytes> files;
  uint64_t time = 0;
};
std::atomic<std::shared_ptr<Backups>> active;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void NoLinks(const fs::path& path) {
  for (auto p = fs::absolute(path); !p.empty();) {
    auto attr = GetFileAttributesW(p.c_str());
    Require(attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_REPARSE_POINT),
            "Save paths must not contain symbolic links or junctions.");
    auto parent = p.parent_path(); if (parent == p) break; p = parent;
  }
}
bool Hex(std::string_view s, size_t length) {
  return s.size() == length && s.find_first_not_of("0123456789abcdefABCDEF") == s.npos;
}
bool SafeRelative(const std::string& text) {
  if (text.empty() || text.size() > 512 || text.find_first_of("\\:*?\"<>|") != text.npos) return false;
  fs::path p(text);
  if (p.is_absolute() || p.has_root_path()) return false;
  for (const auto& part : p) {
    const auto s = part.string();
    if (s.empty() || s == "." || s == ".." || s.back() == '.' || s.back() == ' ') return false;
    if (std::any_of(s.begin(),s.end(),[](unsigned char c) { return c < 32; })) return false;
    auto name=s.substr(0,s.find('.'));
    for(auto& c:name)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if(name=="CON" || name=="PRN" || name=="AUX" || name=="NUL" ||
       (name.size()==4 && (name.starts_with("COM") || name.starts_with("LPT")) && name[3]>='1' && name[3]<='9'))return false;
  }
  return p.generic_string() == text;
}
bool Package(const std::string& text) {
  if (!SafeRelative(text)) return false;
  std::vector<std::string> parts; for (const auto& p : fs::path(text)) parts.push_back(p.string());
  if (parts.size() != 4 || !Hex(parts[0],16) || !Hex(parts[1],8) || parts[2] != "00000001") return false;
  const auto& name=parts[3];
  const std::string prefix="InfiniteUndiscovery_";
  return name.starts_with(prefix) && name.ends_with(".bin") && name.size()==prefix.size()+8 &&
    name.substr(prefix.size(),4).find_first_not_of("0123456789")==std::string::npos;
}
std::string Header(const std::string& package) {
  fs::path p(package);
  return (p.parent_path().parent_path()/"Headers"/"00000001"/(p.filename().string()+".header")).generic_string();
}
struct Handle {
  HANDLE value=INVALID_HANDLE_VALUE;
  explicit Handle(HANDLE h):value(h) {}
  ~Handle(){ if(value!=INVALID_HANDLE_VALUE) CloseHandle(value); }
  Handle(const Handle&)=delete;
};
Bytes Read(const fs::path& path) {
  NoLinks(path);
  // Deny writers and renames while reading; never snapshot an open save writer.
  Handle h(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
  if(h.value==INVALID_HANDLE_VALUE) throw std::system_error(GetLastError(),std::system_category(),"Reading save backup");
  LARGE_INTEGER size{};
  Require(GetFileSizeEx(h.value,&size) && size.QuadPart>=0 && size.QuadPart<=kMaxBytes,"Save file is too large or unreadable.");
  Bytes bytes(static_cast<size_t>(size.QuadPart)); DWORD read=0;
  Require(ReadFile(h.value,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr) && read==bytes.size(),"Incomplete save read.");
  return bytes;
}
void Write(const fs::path& path,const Bytes& data) {
  NoLinks(path); fs::create_directories(path.parent_path());
  Handle h(CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
  if(h.value==INVALID_HANDLE_VALUE) throw std::system_error(GetLastError(),std::system_category(),"Creating backup file");
  DWORD written=0;
  Require(WriteFile(h.value,data.data(),static_cast<DWORD>(data.size()),&written,nullptr) && written==data.size() && FlushFileBuffers(h.value),"Could not finish writing backup.");
}
std::string Hash(const Bytes& bytes) { assets::DigestState digest; digest.Add(bytes.data(),bytes.size()); return digest.Finish(); }
void U32(Bytes& b,uint32_t n) { for(int i=0;i<4;++i)b.push_back(uint8_t(n>>(i*8))); }
void String(Bytes& b,const std::string& s) { U32(b,static_cast<uint32_t>(s.size()));b.insert(b.end(),s.begin(),s.end()); }
struct Reader {
  const Bytes& data; size_t pos=0;
  uint32_t Number() { Require(pos+4<=data.size(),"Truncated backup."); uint32_t n=0;for(int i=0;i<4;++i)n|=uint32_t(data[pos++])<<(i*8);return n; }
  Bytes Blob() { auto size=Number();Require(size<=data.size()-pos,"Truncated backup data.");Bytes b(data.begin()+pos,data.begin()+pos+size);pos+=size;return b; }
  std::string Text() { auto b=Blob();return std::string(b.begin(),b.end()); }
};
Bytes Body(const Snapshot& s) {
  Bytes body; String(body,s.package);U32(body,static_cast<uint32_t>(s.files.size()));
  for(const auto& [name,bytes]:s.files) {String(body,name);U32(body,static_cast<uint32_t>(bytes.size()));body.insert(body.end(),bytes.begin(),bytes.end());}
  Require(body.size()<kMaxBytes-128,"Save package is too large.");return body;
}
Snapshot Load(const fs::path& path) {
  auto bytes=Read(path);Require(bytes.size()>=84 && std::memcmp(bytes.data(),"IUBACK01",8)==0,"Invalid backup format.");
  Snapshot s;for(int i=0;i<8;++i)s.time|=uint64_t(bytes[8+i])<<(i*8);
  Bytes body(bytes.begin()+16,bytes.end()-64);
  s.hash=Hash(body);Require(std::string(bytes.end()-64,bytes.end())==s.hash,"Backup integrity check failed.");
  Reader r{body};s.package=r.Text();Require(Package(s.package),"Invalid save slot in backup.");
  auto count=r.Number();Require(count<=128,"Invalid backup file count.");
  for(uint32_t i=0;i<count;++i) {
    auto name=r.Text();Require(SafeRelative(name) && (name.starts_with(s.package+"/") || name==Header(s.package)),"Unsafe backup path.");
    auto data=r.Blob();Require(s.files.emplace(name,std::move(data)).second,"Duplicate backup path.");
  }
  Require(r.pos==body.size(),"Unexpected backup data.");
  return s;
}
bool Restorable(const Snapshot& s) {
  const auto data=s.files.find(s.package+"/InfiniteUndiscovery.dat");
  return data!=s.files.end() && !data->second.empty();
}
bool FileId(const std::string& id) {
  return id.ends_with(".iubak") && id.size()<100 && id.size()>6 &&
    id.substr(0,id.size()-6).find_first_not_of("0123456789-")==std::string::npos;
}
fs::path Bucket(const std::string& package) {
  fs::path p(package);return p.parent_path().parent_path()/p.filename();
}
bool Id(const std::string& id) {
  if(!SafeRelative(id))return false;
  fs::path p(id);auto bucket=p.parent_path();
  auto package=bucket.parent_path()/"00000001"/bucket.filename();
  return Package(package.generic_string()) && FileId(p.filename().string());
}
std::string NewId() {
  static std::atomic<unsigned> serial{0};
  auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  return std::to_string(ms)+"-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(serial++)+".iubak";
}
std::string Date(uint64_t ms) {
  time_t seconds=static_cast<time_t>(ms/1000);tm t{};localtime_s(&t,&seconds);
  char text[40]{};strftime(text,sizeof(text),"%Y-%m-%d %H:%M:%S",&t);return text;
}
void Move(const fs::path& from,const fs::path& to) {
  NoLinks(from);NoLinks(to);fs::create_directories(to.parent_path());
  if(!MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_WRITE_THROUGH)) throw std::system_error(GetLastError(),std::system_category(),"Moving save file");
}
// Only remove our private transaction directory, after checking every entry.
void Cleanup(const fs::path& transaction) {
  NoLinks(transaction);
  if(!fs::exists(transaction))return;
  for(const auto& entry:fs::recursive_directory_iterator(transaction))NoLinks(entry.path());
  fs::remove_all(transaction);
}
}

Backups::Backups(fs::path profile):profile_(fs::absolute(profile).lexically_normal()),saves_(profile_/"saves"),store_(profile_/"backups"/"saves") {}
void SetActive(std::shared_ptr<Backups> backups) { active.store(std::move(backups)); }
std::shared_ptr<Backups> Active() { return active.load(); }
std::string Backups::Status(){std::lock_guard lock(mutex_);return status_;}
void Backups::Initialize() {
  std::lock_guard lock(mutex_);NoLinks(saves_);NoLinks(store_);fs::create_directories(store_);Recover();
}
void Backups::Capture(const fs::path& package,bool prune) {
  const auto relative=fs::absolute(package).lexically_normal().lexically_relative(saves_).generic_string();
  Require(Package(relative),"Not an Infinite Undiscovery save slot.");
  NoLinks(package); Snapshot s;s.package=relative;
  // Keep all handles open until every file has been copied into memory.
  std::vector<std::unique_ptr<Handle>> locks;
  std::vector<fs::path> paths;
  if(fs::exists(package))for(const auto& entry:fs::recursive_directory_iterator(package)) {
    NoLinks(entry.path());if(entry.is_regular_file())paths.push_back(entry.path());
  }
  auto header=saves_/Header(relative);NoLinks(header);if(fs::exists(header))paths.push_back(header);
  Require(paths.size()<=128,"Too many files in save package.");
  for(const auto& path:paths) {
    auto lock=std::make_unique<Handle>(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(lock->value==INVALID_HANDLE_VALUE)throw std::system_error(GetLastError(),std::system_category(),"Save is busy; backup deferred");
    locks.push_back(std::move(lock));
  }
  size_t total=0;
  for(const auto& path:paths) {auto data=Read(path);total+=data.size();Require(total<kMaxBytes-4096,"Save package is too large.");s.files.emplace(path.lexically_relative(saves_).generic_string(),std::move(data));}
  Require(!prune || Restorable(s),"Save data is empty or missing.");
  auto body=Body(s);s.hash=Hash(body);
  std::vector<std::pair<uint64_t,fs::path>> versions;
  std::string newest_hash;uint64_t newest_time=0;
  const auto bucket=store_/Bucket(relative);NoLinks(bucket);fs::create_directories(bucket);
  for(const auto& entry:fs::directory_iterator(bucket))if(FileId(entry.path().filename().string())) {
    try {auto previous=Load(entry.path());if(previous.package==relative) {
      versions.emplace_back(previous.time,entry.path());if(previous.time>=newest_time){newest_time=previous.time;newest_hash=previous.hash;}
    }}catch(...) {} // Corrupt backups are retained for inspection, never restored.
  }
  if(s.hash==newest_hash){status_="Backups are up to date.";return;}
  s.time=std::max<uint64_t>(newest_time+1,std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  Bytes bytes{'I','U','B','A','C','K','0','1'};for(int i=0;i<8;++i)bytes.push_back(uint8_t(s.time>>(8*i)));
  bytes.insert(bytes.end(),body.begin(),body.end());bytes.insert(bytes.end(),s.hash.begin(),s.hash.end());
  const auto final=bucket/NewId();auto temp=final;temp+=".tmp";
  Write(temp,bytes);Move(temp,final);
  if(prune) {
    std::sort(versions.begin(),versions.end());
    while(versions.size()>=kKeep){NoLinks(versions.front().second);fs::remove(versions.front().second);versions.erase(versions.begin());}
  }
  status_="Last backup: "+Date(s.time)+" (local time).";
}
void Backups::CapturePackage(const fs::path& package) {
  std::lock_guard lock(mutex_);
  try {Capture(package);}catch(const std::exception& e){status_=std::string("Backup not created: ")+e.what();}
}
void Backups::CaptureAll() {
  std::lock_guard lock(mutex_);
  try {
    NoLinks(saves_);if(!fs::exists(saves_)){status_="No saved games yet.";return;}
    // Only walk the known save layout. Do not traverse DLC or other content.
    for(const auto& user:fs::directory_iterator(saves_)) {
      if(!Hex(user.path().filename().string(),16))continue;NoLinks(user.path());if(!user.is_directory())continue;
      for(const auto& title:fs::directory_iterator(user.path())) {
        if(!Hex(title.path().filename().string(),8))continue;auto saved=title.path()/"00000001";NoLinks(saved);if(!fs::exists(saved))continue;
        for(const auto& slot:fs::directory_iterator(saved))if(Package(slot.path().lexically_relative(saves_).generic_string()))Capture(slot.path());
      }
    }
    if(status_.empty())status_="No saved games yet.";
  }catch(const std::exception& e){status_=std::string("Backup not created: ")+e.what();}
}
std::vector<Backup> Backups::List() {
  std::lock_guard lock(mutex_);std::vector<Backup> result;
  try {
    NoLinks(store_);
    for(const auto& entry:fs::recursive_directory_iterator(store_)) {
      NoLinks(entry.path());
      const auto id=entry.path().lexically_relative(store_).generic_string();
      if(!Id(id))continue;
      try {auto s=Load(entry.path());fs::path p(s.package);uint64_t bytes=0;for(const auto& [name,b]:s.files)bytes+=b.size();
        result.push_back({id,p.filename().string(),(*p.begin()).string(),Date(s.time),bytes,s.time,Restorable(s)});
      }catch(const std::exception& e){status_=std::string("A backup could not be read: ")+e.what();}
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.timestamp>b.timestamp;});
  }catch(const std::exception& e){status_=e.what();}
  return result;
}
bool Backups::Validate(const std::string& id,std::string& error) {
  std::lock_guard lock(mutex_);
  try {Require(Id(id),"Invalid backup selection.");Require(Restorable(Load(store_/id)),"This recovery copy has empty or missing game data.");error.clear();return true;}
  catch(const std::exception& e){error=e.what();return false;}
}

void Backups::Recover() {
  const auto txn=store_/"restore-transaction";NoLinks(txn);
  if(!fs::exists(txn))return;
  if(!fs::exists(txn/"plan") || fs::exists(txn/"committed")){Cleanup(txn);return;}
  auto bytes=Read(txn/"plan");Reader reader{bytes};auto relative=reader.Text();
  Require(Package(relative),"Invalid restore recovery journal.");
  const bool had_package=reader.Number()!=0,had_header=reader.Number()!=0;
  auto rollback=[&](const fs::path& live,const char* old,const char* staged,const char* rejected,bool had) {
    NoLinks(live);
    if(fs::exists(txn/old)) {
      if(fs::exists(live))Move(live,txn/rejected);
      Move(txn/old,live);
    }else if(!had && !fs::exists(txn/staged) && fs::exists(live))Move(live,txn/rejected);
  };
  rollback(saves_/relative,"old-package","new-package","rejected-package",had_package);
  rollback(saves_/Header(relative),"old-header","new-header","rejected-header",had_header);
  Cleanup(txn);status_="Recovered the previous save after an interrupted restore.";
}
bool Backups::Restore(const std::string& id,std::string& error) {
  std::lock_guard lock(mutex_);
  try {
    Require(Id(id),"Invalid backup selection.");Recover();auto selected=Load(store_/id);
    Require(Restorable(selected),"This recovery copy has empty or missing game data.");
    auto package=saves_/selected.package,header=saves_/Header(selected.package);
    NoLinks(package);NoLinks(header);
    // Capture current data before touching it, even if it exceeds retention by one.
    if(fs::exists(package) || fs::exists(header))Capture(package,false);
    const auto txn=store_/"restore-transaction";NoLinks(txn);fs::create_directory(txn);
    for(const auto& [name,data]:selected.files) {
      auto path=name==Header(selected.package)?txn/"new-header":txn/"new-package"/fs::path(name).lexically_relative(selected.package);
      Write(path,data);
    }
    Bytes plan;String(plan,selected.package);U32(plan,fs::exists(package));U32(plan,fs::exists(header));Write(txn/"plan.tmp",plan);Move(txn/"plan.tmp",txn/"plan");
    if(restore_checkpoint)restore_checkpoint(0);
    if(fs::exists(package))Move(package,txn/"old-package");
    if(restore_checkpoint)restore_checkpoint(1);
    if(fs::exists(header))Move(header,txn/"old-header");
    if(restore_checkpoint)restore_checkpoint(2);
    Move(txn/"new-package",package);
    if(restore_checkpoint)restore_checkpoint(3);
    if(fs::exists(txn/"new-header"))Move(txn/"new-header",header);
    if(restore_checkpoint)restore_checkpoint(4);
    Write(txn/"committed",{});Cleanup(txn);status_="Backup restored. The previous save is retained in backups.";error.clear();return true;
  }catch(const std::exception& e){
    error=e.what();try{Recover();}catch(const std::exception& recovery){error+=" Recovery is pending: ";error+=recovery.what();}
    status_="Restore failed: "+error;return false;
  }
}
} // namespace iu::saves
