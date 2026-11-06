#pragma once
#include "save_anywhere_state.h"
#include "save_activity.h"
#include "guest_memory.h"
#include <atomic>

DECLARE_REX_FUNC(sub_82475670);
DECLARE_REX_FUNC(sub_8219CAF0);
DECLARE_REX_FUNC(sub_826D7E08);
DECLARE_REX_FUNC(sub_8247CC38);
DECLARE_REX_FUNC(sub_82499198);

namespace iu::save_anywhere {
inline constexpr uint32_t kGlobal=0x82A58EA8;
inline std::atomic<uint32_t> dialog_handle{0}, dialog_object{0};
inline uint32_t owner_service=0; // Frame thread only.
inline bool IsActive() { return dialog_handle.load()!=0; }
inline uint32_t GetDialogHandle() { return dialog_handle.load(); }
inline uint32_t GetDialogObject() { return dialog_object.load(); }
inline bool Request() { return requests.Request(); }

struct GuestReader {
  uint8_t* base;
  bool U32(uint32_t address,uint32_t& result) {
    if (!guest_memory::readable(base,address,4)) return false;
    result=guest_memory::read32(base,address); return true;
  }
  bool U8(uint32_t address,uint32_t& result) {
    if (!guest_memory::readable(base,address,1)) return false;
    result=*guest_memory::host_ptr(base,address); return true;
  }
  bool Resolve(uint32_t handle,uint32_t& object) {
    uint32_t table, actual, types;
    if (!U32(0x82A380F8+24,table) || !table) return false;
    const uint64_t slot=uint64_t(table)+180+uint64_t(handle&0xffff)*4;
    if (slot>UINT32_MAX || !U32(uint32_t(slot),object)) return false;
    if (!object) return true;
    if (!U32(object+16,actual) || !U32(object+28,types)) return false;
    if (actual!=handle || !(types&(1u<<7))) object=0;
    return true;
  }
};

// Game frame thread only. Guest calls use a copied context and a separate
// caller/linkage area; nothing runs from the UI or fiber-switch callbacks.
inline void PollAndDispatch(PPCContext& ctx,uint8_t* base) {
  GuestReader memory{base};
  auto snapshot=Inspect(memory);
  const auto request=requests.Take();
  if (saves::activity.Busy() || saves::activity.ExitReserved()) snapshot.availability=Availability::Saving;
  const uint32_t original_sp=ctx.r1.u32;
  const uint32_t stack=original_sp>=0x10100 ? (original_sp-256)&~15u : 0;
  const bool valid_stack=stack && guest_memory::readable(base,stack,256);
  if (IsActive()) {
    uint32_t object=0, service=0;
    if (valid_stack && !saves::activity.Busy() && memory.Resolve(GetDialogHandle(),object) && !object &&
        memory.U32(kGlobal+8,service) && service==owner_service) {
      PPCContext call=ctx;
      REX_STORE_U32(stack,original_sp);
      call.r1.u64=stack; call.r3.u64=kGlobal+404; call.r4.u64=0;
      sub_8247CC38(call,base);
      call.r1.u64=stack; call.r3.u64=service; call.r4.u64=1;
      sub_82499198(call,base);
      dialog_object.store(0); dialog_handle.store(0); owner_service=0;
      snapshot=Inspect(memory);
    } else snapshot.availability=Availability::Active;
    requests.Publish(snapshot);
    return;
  }
  if (!request) { requests.Publish(snapshot); return; }
  if (snapshot.availability!=Availability::Ready || !request->SameSession(snapshot) || !valid_stack) {
    if (snapshot.availability==Availability::Ready) snapshot.availability=Availability::Changed;
    requests.Publish(snapshot); return;
  }
  uint32_t service;
  if (!memory.U32(kGlobal+8,service) || !service) return;
  // Native save-point contract from sub_8263CBB8. The holder must be above
  // the 80-byte PPC linkage/argument area, which callees may overwrite.
  const uint32_t holder=stack+96;
  REX_STORE_U32(stack,original_sp);
  REX_STORE_U32(holder,0); REX_STORE_U32(holder+4,0);
  PPCContext call=ctx;
  call.r1.u64=stack; call.r3.u64=holder; call.r6.u64=0; call.r7.u64=0;
  call.fpscr.disableFlushMode(); call.f1.f64=58.0; call.f2.f64=27.0;
  sub_82475670(call,base);
  const uint32_t handle=REX_LOAD_U32(holder+4);
  uint32_t object=0;
  if (!handle || !memory.Resolve(handle,object) || !object) {
    snapshot.availability=Availability::Failed; requests.Publish(snapshot); return;
  }
  REX_STORE_U32(object+12592,1);
  call.r1.u64=stack; call.r3.u64=object; call.r4.u64=1;
  sub_8219CAF0(call,base);
  call.r1.u64=stack; call.r3.u64=object;
  sub_826D7E08(call,base);
  call.r1.u64=stack; call.r3.u64=kGlobal+404; call.r4.u64=1;
  sub_8247CC38(call,base);
  call.r1.u64=stack; call.r3.u64=service; call.r4.u64=0;
  sub_82499198(call,base);
  owner_service=service;
  dialog_object.store(object); dialog_handle.store(handle);
  snapshot.availability=Availability::Active; requests.Publish(snapshot);
}
} // namespace iu::save_anywhere
