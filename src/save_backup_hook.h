#pragma once
#include "save_backups.h"
#include "guest_memory.h"
#include <rex/system/kernel_state.h>
#include <rex/system/xam/content_manager.h>
#include <windows.h>

extern "C" void __imp__XamContentClose(PPCContext& ctx,uint8_t* base) {
  static auto* original=[] {
    auto proc=GetProcAddress(GetModuleHandleW(L"rexruntime.dll"),"__imp__XamContentClose");
    if(!proc)std::abort();return reinterpret_cast<PPCFunc*>(proc);
  }();
  const auto overlapped=ctx.r4.u32;
  std::filesystem::path package;
  auto backups=iu::saves::Active();
  if(backups)try {
    std::string root;
    for(uint32_t i=0;i<64;++i) {
      if(uint64_t(ctx.r3.u32)+i>UINT32_MAX || !iu::guest_memory::readable(base,ctx.r3.u32+i,1)){root.clear();break;}
      auto c=static_cast<char>(REX_LOAD_U8(ctx.r3.u32+i));if(!c)break;root+=c;
    }
    if(!root.empty() && root.size()<64) {
      // GetOpenPackagePath does not lock its package map internally.
      // Match the SDK's map lock, then release it before any filesystem I/O.
      auto lock=rex::thread::global_critical_region::AcquireDirect();
      package=rex::system::kernel_state()->content_manager()->GetOpenPackagePath(root);
    }
  }catch(...) {} // Backups must never prevent the game's content close.
  original(ctx,base);
  bool closed=ctx.r3.u32==0;
  if(ctx.r3.u32==997 && overlapped && iu::guest_memory::readable(base,overlapped,4))
    closed=REX_LOAD_U32(overlapped)==0; // SDK completes this call synchronously.
  if(closed && backups && package.parent_path().filename()=="00000001" &&
     package.filename().string().starts_with("InfiniteUndiscovery_"))
    backups->CapturePackage(package);
}
