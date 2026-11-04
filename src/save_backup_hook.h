#pragma once
#include "save_backups.h"
#include "save_activity.h"
#include "guest_memory.h"
#include <rex/system/kernel_state.h>
#include <rex/system/xam/content_manager.h>
#include <windows.h>

namespace iu::saves::hooks {
// Serialize guest mount/close wrappers, including their bookkeeping. The SDK
// still owns deferred work; do not hold its global critical region over I/O.
inline std::recursive_mutex content_calls;
inline PPCFunc* Original(const char* name) {
  auto proc=GetProcAddress(GetModuleHandleW(L"rexruntime.dll"),name);
  if(!proc)std::abort();
  return reinterpret_cast<PPCFunc*>(proc);
}
inline std::string Root(uint8_t* base,uint32_t address) {
  std::string root;
  for(uint32_t i=0;i<64;++i) {
    if(uint64_t(address)+i>UINT32_MAX || !iu::guest_memory::readable(base,address+i,1))return {};
    auto c=static_cast<char>(REX_LOAD_U8(address+i));
    if(!c)return root;
    root+=c;
  }
  return {};
}
inline bool SaveContent(uint8_t* base,uint32_t address) {
  // XCONTENT_DATA: device at +0, content type at +4 (saved game = 1).
  return iu::guest_memory::readable(base,address,8) && REX_LOAD_U32(address+4)==1;
}
inline Activity::Result FailedOpen(const std::string& root) {
  auto lock=rex::thread::global_critical_region::AcquireDirect();
  return rex::system::kernel_state()->content_manager()->GetOpenPackagePath(root).empty()
      ? Activity::Result::RootClosed : Activity::Result::Failed;
}
struct Operation {
  Activity::Id id=activity.Begin();
  ~Operation(){activity.End(id);}
};
} // namespace iu::saves::hooks

// This is the only content-create import used by the supported title. Cover
// OPEN_EXISTING as well: it is used for loading and may also allow writes.
extern "C" void __imp__XamContentCreateEx(PPCContext& ctx,uint8_t* base) {
  using namespace iu::saves;
  static auto* original=hooks::Original("__imp__XamContentCreateEx");
  if(!hooks::SaveContent(base,ctx.r5.u32)){original(ctx,base);return;}
  std::lock_guard serialize(hooks::content_calls);
  const auto root=hooks::Root(base,ctx.r4.u32);
  if(root.empty()){original(ctx,base);return;}
  // ABI argument 9 is at r1+0x54; r10 holds the 64-bit content size.
  uint32_t overlapped=0;
  if(uint64_t(ctx.r1.u32)+0x58<=0x100000000ull &&
     iu::guest_memory::readable(base,ctx.r1.u32+0x54,4))overlapped=REX_LOAD_U32(ctx.r1.u32+0x54);
  const auto id=activity.Begin(root);
  try { original(ctx,base); } catch(...) {activity.End(id);throw;}
  if(ctx.r3.u32==997) {
    activity.Pending(id,[base,overlapped,root]() {
      if(!iu::guest_memory::readable(base,overlapped,4))return Activity::Result::Pending; // Fail closed.
      const auto result=iu::guest_memory::read32(base,overlapped);
      if(result==997)return Activity::Result::Pending;
      // The title may have reused the overlapped for another operation before
      // the UI polls it. A mounted root stays protected regardless of that
      // later result; only an absent mount can release this session here.
      return hooks::FailedOpen(root)==Activity::Result::RootClosed
          ? Activity::Result::RootClosed : Activity::Result::Open;
    });
  } else if(ctx.r3.u32!=0) {
    const auto result=hooks::FailedOpen(root);
    activity.Pending(id,[result] { return result; });
    activity.Refresh();
  }
}

// In the pinned SDK these complete synchronously, even with an overlapped
// result. Protect thumbnail changes and file writes/flushes outside a mount.
extern "C" void __imp__XamContentSetThumbnail(PPCContext& ctx,uint8_t* base) {
  static auto* original=iu::saves::hooks::Original("__imp__XamContentSetThumbnail");
  if(!iu::saves::hooks::SaveContent(base,ctx.r4.u32)){original(ctx,base);return;}
  iu::saves::hooks::Operation operation;
  original(ctx,base);
}
extern "C" void __imp__NtWriteFile(PPCContext& ctx,uint8_t* base) {
  static auto* original=iu::saves::hooks::Original("__imp__NtWriteFile");
  iu::saves::hooks::Operation operation;
  original(ctx,base);
}
extern "C" void __imp__NtFlushBuffersFile(PPCContext& ctx,uint8_t* base) {
  static auto* original=iu::saves::hooks::Original("__imp__NtFlushBuffersFile");
  iu::saves::hooks::Operation operation;
  original(ctx,base);
}

extern "C" void __imp__XamContentClose(PPCContext& ctx,uint8_t* base) {
  static auto* original=iu::saves::hooks::Original("__imp__XamContentClose");
  std::lock_guard serialize(iu::saves::hooks::content_calls);
  iu::saves::activity.Refresh();
  const auto root=iu::saves::hooks::Root(base,ctx.r3.u32);
  const auto sessions=iu::saves::activity.ForRoot(root);
  // Covers unmount and automatic backup until both have finished, including
  // containers opened before tracking began. Failed closes retain the session.
  iu::saves::hooks::Operation closing;
  const auto overlapped=ctx.r4.u32;
  std::filesystem::path package;
  auto backups=iu::saves::Active();
  if(backups)try {
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
  if(closed)iu::saves::activity.Closed(sessions);
}
