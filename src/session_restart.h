#pragma once
#include <windows.h>
#include <charconv>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace iu::session {
inline std::wstring Quote(const std::wstring& text) {
  std::wstring out=L"\"";size_t slashes=0;
  for(wchar_t c:text) {
    if(c==L'\\'){++slashes;continue;}
    if(c==L'\"')out.append(slashes*2+1,L'\\');else out.append(slashes,L'\\');
    slashes=0;out+=c;
  }
  out.append(slashes*2,L'\\');return out+L"\"";
}
inline void NoLinks(const std::filesystem::path& path) {
  for(auto p=std::filesystem::absolute(path);!p.empty();) {
    auto a=GetFileAttributesW(p.c_str());
    if(a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Session path contains a link or junction.");
    auto up=p.parent_path();if(up==p)break;p=up;
  }
}
class ProfileLock {
 public:
  ~ProfileLock(){if(handle_!=INVALID_HANDLE_VALUE)CloseHandle(handle_);}
  void Acquire(const std::filesystem::path& profile) {
    const auto file=profile/"cache"/"session.lock";NoLinks(file);
    std::filesystem::create_directories(file.parent_path());
    handle_=CreateFileW(file.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle_==INVALID_HANDLE_VALUE)throw std::system_error(GetLastError(),std::system_category(),"This profile is already running or cannot be locked");
  }
 private: HANDLE handle_=INVALID_HANDLE_VALUE;
};
inline bool WaitForPreviousProcess(const std::string& value,std::string& error) {
  if(value.empty())return true;
  uintptr_t number=0;auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),number);
  if(ec!=std::errc{} || end!=value.data()+value.size() || !number){error="Invalid restart handoff.";return false;}
  auto handle=reinterpret_cast<HANDLE>(number);
  auto result=WaitForSingleObject(handle,30000);CloseHandle(handle);
  if(result!=WAIT_OBJECT_0){error="The previous game session did not exit. Restart cancelled; saves were not changed.";return false;}
  return true;
}
// Only this process handle is inherited. The child waits for actual process
// exit, including guest stragglers and open save handles, before touching saves.
inline bool Launch(const std::filesystem::path& exe,const std::vector<std::wstring>& args,std::string& error) {
  HANDLE parent=OpenProcess(SYNCHRONIZE,TRUE,GetCurrentProcessId());
  if(!parent){error="Could not prepare restart handoff.";return false;}
  SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);
  std::vector<uint8_t> storage(size);
  auto* list=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
  bool initialized=InitializeProcThreadAttributeList(list,1,0,&size)!=FALSE;
  bool success=false;
  if(initialized && UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,&parent,sizeof(parent),nullptr,nullptr)) {
    STARTUPINFOEXW si{};si.StartupInfo.cb=sizeof(si);si.lpAttributeList=list;
    std::wstring cmd=Quote(exe.wstring());for(const auto& arg:args)cmd+=L" "+Quote(arg);
    cmd+=L" --restart_wait_handle="+std::to_wstring(reinterpret_cast<uintptr_t>(parent));
    PROCESS_INFORMATION process{};
    success=CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW,nullptr,exe.parent_path().c_str(),&si.StartupInfo,&process)!=FALSE;
    if(success){CloseHandle(process.hThread);CloseHandle(process.hProcess);}
  }
  const auto failure=GetLastError();if(initialized)DeleteProcThreadAttributeList(list);CloseHandle(parent);
  if(!success)error="Could not launch the game. Windows error "+std::to_string(failure)+". The current session is still running.";
  else error.clear();return success;
}
} // namespace iu::session
