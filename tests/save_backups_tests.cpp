#include "../src/save_backups.h"
#include "../src/session_restart.h"
#include "../src/asset_hash.h"
#include <fstream>
#include <iostream>
#include <cstdlib>

namespace fs=std::filesystem;
static void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void Put(const fs::path& path,const std::string& bytes){fs::create_directories(path.parent_path());std::ofstream f(path,std::ios::binary);f<<bytes;f.close();Check(bool(f),"Write fixture");}
static std::string Get(const fs::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static fs::path Exe(){wchar_t p[32768]{};GetModuleFileNameW(nullptr,p,32768);return p;}
static DWORD Run(const std::vector<std::wstring>& args){
  auto exe=Exe();std::wstring command=iu::session::Quote(exe.wstring());for(const auto& a:args)command+=L" "+iu::session::Quote(a);
  STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
  Check(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi),"Launch test helper");
  CloseHandle(pi.hThread);Check(WaitForSingleObject(pi.hProcess,20000)==WAIT_OBJECT_0,"Helper completed");DWORD result=0;GetExitCodeProcess(pi.hProcess,&result);CloseHandle(pi.hProcess);return result;
}
int main(int argc,char** argv){
  try {
    if(argc==4 && std::string(argv[1])=="--crash-restore") {
      iu::saves::Backups backups(argv[2]);backups.Initialize();backups.restore_checkpoint=[](int step){if(step==3)std::_Exit(88);};
      std::string error;backups.Restore(argv[3],error);return 2;
    }
    if(argc>=4 && std::string(argv[1])=="--restart-child") {
      std::string handoff;
      for(int i=3;i<argc;++i){std::string a=argv[i];if(a.starts_with("--restart_wait_handle="))handoff=a.substr(22);}
      std::string error;Check(!handoff.empty() && iu::session::WaitForPreviousProcess(handoff,error),"Wait for parent process exit");
      fs::path root(argv[2]);iu::session::ProfileLock lock;lock.Acquire(root/"USA");
      Put(root/"child-result",fs::current_path().string());return 0;
    }
    if(argc==3 && std::string(argv[1])=="--restart-parent") {
      fs::path root(argv[2]);iu::session::ProfileLock lock;lock.Acquire(root/"USA");
      std::string error;Check(iu::session::Launch(Exe(),{L"--restart-child",root.wstring()},error),"Start restart helper");
      // Child must still be waiting while this process holds the profile lock.
      Sleep(100);Check(!fs::exists(root/"child-result"),"No overlapping game sessions");return 0;
    }
    const auto root=fs::current_path()/("save-backup-tests-"+std::to_string(GetCurrentProcessId()));
    Check(!fs::exists(root),"Unique test root");fs::create_directories(root);
    const std::string relative="B13EBABEBABEBABE/535107DB/00000001/InfiniteUndiscovery_0003.bin";
    const auto profile=root/"USA",package=profile/"saves"/relative;
    const auto data=package/"InfiniteUndiscovery.dat";
    const auto header=profile/"saves/B13EBABEBABEBABE/535107DB/Headers/00000001/InfiniteUndiscovery_0003.bin.header";
    Put(data,"SAVE A");Put(header,"HEADER A");Put(package/"__thumbnail.png","THUMB A");
    const auto unrelated=profile/"saves/B13EBABEBABEBABE/535107DB/00000002/dlc/file";Put(unrelated,"DLC");
    iu::saves::Backups backups(profile);backups.Initialize();backups.CaptureAll();
    auto list=backups.List();Check(list.size()==1,"Back up existing slot at startup");const auto original=list[0].id;
    backups.CaptureAll();Check(backups.List().size()==1,"Unchanged saves do not consume retention");
    Put(data,"SAVE B");Put(header,"HEADER B");backups.CapturePackage(package);
    Check(backups.List().size()==2,"Capture changed save");
    std::string error;Check(backups.Restore(original,error),error.c_str());
    Check(Get(data)=="SAVE A" && Get(header)=="HEADER A" && Get(package/"__thumbnail.png")=="THUMB A","Restore data and metadata together");
    Check(Get(unrelated)=="DLC" && backups.List().size()==2,"Restore preserves other content and recovery version");
    Put(data,"");Check(backups.Restore(original,error) && Get(data)=="SAVE A","A truncated current save does not block recovery");
    Check(!backups.List().front().can_restore,"Incomplete current save retained as a recovery copy");
    for(int failure=0;failure<=4;++failure) {
      Put(data,"CURRENT");Put(header,"CURRENT HEADER");
      backups.restore_checkpoint=[failure](int step){if(step==failure)throw std::runtime_error("Injected I/O failure");};
      Check(!backups.Restore(original,error),"Restore failure surfaces");
      Check(Get(data)=="CURRENT" && Get(header)=="CURRENT HEADER","Rollback preserves current save at every transition");
    }
    backups.restore_checkpoint={};
    Check(Run({L"--crash-restore",profile.wstring(),std::wstring(original.begin(),original.end())})==88,"Simulate process loss during restore");
    backups.Initialize();Check(Get(data)=="CURRENT" && Get(header)=="CURRENT HEADER","Startup recovers interrupted restore");
    const auto count=backups.List().size();
    HANDLE writer=CreateFileW(data.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    Check(writer!=INVALID_HANDLE_VALUE,"Hold save writer open");backups.CapturePackage(package);CloseHandle(writer);
    Check(backups.List().size()==count && backups.Status().find("not created")!=std::string::npos,"Never copy a save while a writer is open");
    Check(!backups.Restore("../outside.iubak",error),"Reject traversal selection");
    const auto original_file=profile/"backups/saves"/original;auto corrupt=Get(original_file);corrupt[20]^=1;Put(original_file,corrupt);
    Check(!backups.Validate(original,error) && !backups.Restore(original,error) && Get(data)=="CURRENT","Reject corruption without touching current save");
    corrupt[20]='.';corrupt[21]='.';corrupt[22]='/';
    iu::assets::DigestState digest;digest.Add(corrupt.data()+16,corrupt.size()-80);
    corrupt.replace(corrupt.size()-64,64,digest.Finish());Put(original_file,corrupt);
    Check(!backups.Validate(original,error) && Get(data)=="CURRENT","Reject a correctly hashed backup containing path traversal");
    for(int n=0;n<13;++n){Put(data,"VERSION "+std::to_string(n));backups.CapturePackage(package);}
    Check(backups.List().size()==10,"Retain ten valid distinct versions per slot");
    const auto latest=backups.List().front().id;Put(data,"NEWEST UNSNAPSHOTTED");
    Check(backups.Restore(latest,error) && Get(data)=="VERSION 12","Preserve unsnapshotted current save before restoring");
    Check(backups.List().size()==11,"Recovery copy is not pruned during restore");
    const auto second_package=package.parent_path()/"InfiniteUndiscovery_0004.bin";
    const auto second_header=header.parent_path()/"InfiniteUndiscovery_0004.bin.header";
    Put(second_package/"InfiniteUndiscovery.dat","SECOND SLOT");backups.CapturePackage(second_package);
    auto second_list=backups.List();auto second=std::find_if(second_list.begin(),second_list.end(),[](const auto& b){return b.slot=="InfiniteUndiscovery_0004.bin";});
    Check(second!=second_list.end(),"Second slot has independent backup history");
    auto second_id=second->id;Put(second_header,"NEW HEADER");
    Check(backups.Restore(second_id,error) && !fs::exists(second_header),"Restore absence of optional metadata, without stale newer header");
    fs::remove_all(second_package);Put(second_header,"ORPHAN HEADER");
    Check(backups.Restore(second_id,error) && Get(second_package/"InfiniteUndiscovery.dat")=="SECOND SLOT" && !fs::exists(second_header),"Recover a deleted slot and preserve orphan metadata in recovery backup");
    Check(Get(data)=="VERSION 12","Restoring another slot does not modify existing progress");
    iu::saves::Backups other(root/"EUROPE");other.Initialize();Check(!other.Restore(latest,error),"Backups stay in their content profile");
    {
      iu::session::ProfileLock first;first.Acquire(profile);bool rejected=false;
      try{iu::session::ProfileLock second;second.Acquire(profile);}catch(...){rejected=true;}
      Check(rejected,"Prevent concurrent sessions for the same profile");
    }
    Check(!iu::session::Launch(root/"missing.exe",{},error) && !error.empty(),"Launch failure reported without exiting current process");
    Check(Run({L"--restart-parent",root.wstring()})==0,"Restart parent helper");
    for(int i=0;i<200 && !fs::exists(root/"child-result");++i)Sleep(25);
    Check(Get(root/"child-result")==Exe().parent_path().string(),"Restart waits for exit and uses executable working directory");
    // Fixture root was created above under the test working directory.
    Check(root.parent_path()==fs::current_path(),"Cleanup confined to fixture root");fs::remove_all(root);
    std::cout<<"PASS backup retention, deduplication, metadata, integrity, writer locks, restore rollback/crash recovery, profile isolation, restart handoff\n";
    return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
