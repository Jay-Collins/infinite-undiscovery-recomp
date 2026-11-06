#include "../src/save_anywhere_state.h"
#include <iostream>
#include <map>
#include <stdexcept>
#include <set>

struct Register { union { uint64_t u64=0; uint32_t u32; }; };
struct PPCContext {
  Register r1,r3,r4,r6,r7;
  struct { double f64=0; } f1,f2;
  struct { void disableFlushMode() {} } fpscr;
};
#define DECLARE_REX_FUNC(name) void name(PPCContext&,uint8_t*)
#define REX_STORE_U32(address,value) (*reinterpret_cast<uint32_t*>(base+(address))=__builtin_bswap32(uint32_t(value)))
#define REX_LOAD_U32(address) __builtin_bswap32(*reinterpret_cast<uint32_t*>(base+(address)))
#include "../src/save_anywhere.h"

static unsigned created=0, activated=0, focused=0, modal_on=0, modal_off=0, enabled=0;
void sub_82475670(PPCContext& ctx,uint8_t* base) {
  ++created;
  if(ctx.r3.u64!=ctx.r1.u64+96 || ctx.r6.u64 || ctx.r7.u64 || ctx.f1.f64!=58 || ctx.f2.f64!=27)
    throw std::runtime_error("incorrect native window arguments");
  // Overwrite ABI parameter space as a real guest call may do.
  for(unsigned i=8;i<80;i+=4) REX_STORE_U32(ctx.r1.u32+i,0xdeadbeef);
  REX_STORE_U32(ctx.r3.u32+4,99);
  REX_STORE_U32(0x100000+180+99*4,0x700000);
  REX_STORE_U32(0x700000+16,99);REX_STORE_U32(0x700000+28,1u<<7);
}
void sub_8219CAF0(PPCContext& ctx,uint8_t*) {
  if(ctx.r3.u64!=0x700000 || ctx.r4.u64!=1)throw std::runtime_error("incorrect activate arguments");
  ++activated;
}
void sub_826D7E08(PPCContext& ctx,uint8_t*) {
  if(ctx.r3.u64!=0x700000)throw std::runtime_error("incorrect focus arguments");
  ++focused;
}
void sub_8247CC38(PPCContext& ctx,uint8_t*) {
  if(ctx.r3.u64!=0x82A58EA8+404)throw std::runtime_error("incorrect modal global address");
  if(ctx.r4.u64)++modal_on;else ++modal_off;
}
void sub_82499198(PPCContext& ctx,uint8_t*) {
  if(ctx.r3.u64!=0x210000)throw std::runtime_error("incorrect save manager");
  if(ctx.r4.u64)++enabled;
}
using namespace iu::save_anywhere;
struct Memory {
  std::map<uint32_t,uint32_t> words, bytes;
  bool U32(uint32_t p,uint32_t& v) { auto i=words.find(p); if(i==words.end())return false;v=i->second;return true; }
  bool U8(uint32_t p,uint32_t& v) { auto i=bytes.find(p); if(i==bytes.end())return false;v=i->second;return true; }
  void Object(uint32_t handle,uint32_t object,unsigned type) {
    words[0x100000+180+4*(handle&0xffff)]=object;
    words[object+16]=handle; words[object+28]=1u<<type;
  }
  Memory() {
    words[0x82A58EA8+112]=0; words[0x82A380F8+24]=0x100000;
    words[0x82A380F8+48]=0x200000; words[0x200000+212]=0;
    words[0x82A58EA8+8]=0x210000; bytes[0x210000+64]=1; bytes[0x210000+72]=1;
    words[0x82A95998]=0x220000; words[0x220000+60]=0;
    words[0x82A58EA8+120]=10; words[0x82A58EA8+124]=1;
    Object(10,0x300000,19); Object(11,0x310000,19);
    Object(1,0x400000,21); Object(201,0x500000,20);
    words[0x400000+76]=0x410000; words[0x410000+132]=0; words[0x410000+144]=0;
    words[0x400000+92]=0x420000; words[0x400000+96]=0x420008;
    words[0x420000]=10; words[0x420004]=11;
    words[0x300000+3260]=0x600000; words[0x310000+3260]=0x610000;
    words[0x600000+48]=100; words[0x610000+48]=100;
    words[0x600000+8]=0; words[0x610000+8]=0;
  }
};
void Check(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }

// Exercise the production dispatcher against real readable guest mappings,
// replacing only native game calls. The reserved arena consumes virtual space;
// only fixture pages are committed. No game files or saves are used.
void DispatchTests(const Memory& fixture) {
  struct Arena {
    uint8_t* base=static_cast<uint8_t*>(VirtualAlloc(nullptr,0x100000000ull,MEM_RESERVE,PAGE_NOACCESS));
    std::set<uint32_t> pages;
    ~Arena(){if(base)VirtualFree(base,0,MEM_RELEASE);}
    void Commit(uint32_t address) {
      Check(base!=nullptr,"reserve guest arena");
      uint32_t page=address&~0xfffu;
      if(pages.insert(page).second)Check(VirtualAlloc(base+page,4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"commit guest page");
    }
    void Word(uint32_t address,uint32_t value){Commit(address);REX_STORE_U32(address,value);}
  } arena;
  for(auto [p,v]:fixture.words)arena.Word(p,v);
  for(auto [p,v]:fixture.bytes){arena.Commit(p);arena.base[p]=uint8_t(v);}
  arena.Word(0x100000+180+99*4,0);arena.Word(0x700000+16,0);
  arena.Commit(0x700000+12592);arena.Commit(0x80f000);
  PPCContext ctx;ctx.r1.u64=0x810000;ctx.r3.u64=0xfedcba9800000042ull;
  auto tick=[&]{PollAndDispatch(ctx,arena.base);Check(ctx.r3.u64==0xfedcba9800000042ull,"caller registers changed");};
  tick();Check(Request(),"request safe mapped game");
  arena.Word(0x82A58EA8+112,2);tick();
  Check(created==0,"cutscene entered after click must prevent creation");
  arena.Word(0x82A58EA8+112,0);tick();
  Check(created==0,"rejected request must not reopen after cutscene");
  Check(Request(),"request before combat transition");arena.Word(0x600008,1u<<14);tick();
  Check(created==0,"combat entered after click must prevent creation");
  arena.Word(0x600008,0);tick();
  Check(Request(),"request before save activity");auto busy=iu::saves::activity.Begin();tick();
  Check(created==0,"save activity entered after click must prevent creation");
  iu::saves::activity.End(busy);tick();Check(Request(),"request native window");tick();
  Check(created==1 && activated==1 && focused==1 && modal_on==1 && IsActive(),"native dialog did not activate exactly once");
  Check(!Request(),"duplicate active request must reject");
  for(unsigned i=0;i<5;++i)tick();
  Check(modal_off==0 && enabled==0,"live native dialog must remain locked");
  busy=iu::saves::activity.Begin();arena.Word(0x100000+180+99*4,0);tick();
  Check(IsActive() && !modal_off,"cleanup must wait for pending save I/O");
  iu::saves::activity.End(busy);tick();
  Check(!IsActive() && modal_off==1 && enabled==1,"dismissal must restore modal/input exactly once");
  tick();Check(modal_off==1 && enabled==1,"cleanup must not repeat");
}
int main() {
  try {
    Memory safe;
    const auto ready=Inspect(safe);
    Check(ready.availability==Availability::Ready,"exploration should allow saving");
    for(unsigned bit: {0,1,2,3,5,6,11,12,15,24,29,30,31}) {
      auto m=safe;m.words[0x82A58EA8+112]=1u<<bit;
      Check(Inspect(m).availability==Availability::Event,"event/transition must block");
    }
    for(uint32_t address: {0x600008u,0x610008u}) {
      auto m=safe;m.words[address]=1u<<14;
      Check(Inspect(m).availability==Availability::Combat,"any living party member in battle must block");
    }
    for(uint32_t address: {0x410084u,0x410090u}) {
      auto m=safe;m.words[address]=1;
      Check(Inspect(m).availability==Availability::Combat,"hostile targets must block with weapons sheathed");
    }
    auto menu=safe;menu.words[0x200000+212]=1;
    Check(Inspect(menu).availability==Availability::Menu,"native menu must block");
    auto stale=safe;stale.words[0x300000+16]=0x1000a;
    Check(Inspect(stale).availability==Availability::Unknown,"stale object generation must block");
    auto dead=safe;dead.words[0x600000+48]=0;
    Check(Inspect(dead).availability==Availability::Unknown,"dead controlled actor must block");
    // Missing any required read fails closed, rather than treating it as zero.
    for(auto [address,value]:safe.words) {
      auto m=safe;m.words.erase(address);
      Check(Inspect(m).availability!=Availability::Ready,"unreadable word must block");
    }
    for(auto [address,value]:safe.bytes) {
      auto m=safe;m.bytes.erase(address);
      Check(Inspect(m).availability!=Availability::Ready,"unreadable byte must block");
    }
    Requests queue;
    Check(!queue.Request(),"startup request must be rejected");
    queue.Publish(ready);Check(queue.Request(),"safe request should queue");
    Check(!queue.Request(),"duplicate request must be rejected");
    auto pending=queue.Take();Check(pending && pending->SameSession(ready),"request should preserve session identity");
    Check(!queue.Take(),"consumed request must never fire later");
    auto changed=ready;changed.world++;
    Check(!pending->SameSession(changed),"map transition must invalidate pending request");
    DispatchTests(safe);
    std::cout<<"Save Anywhere safety checks passed\n";
    return 0;
  } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
