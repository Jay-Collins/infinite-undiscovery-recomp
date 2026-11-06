#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <rex/logging.h>
struct Register { union { uint64_t u64=0; uint32_t u32; }; };
struct PPCContext { Register r1,r3,r4; uint64_t lr=0; };
#define REX_STORE_U8(address,value) (base[address]=uint8_t(value))
#include "../src/difficulty_unlock.h"

static unsigned original_calls=0;
static uint64_t original_result=0;
extern "C" void __imp__sub_82499008(PPCContext& ctx,uint8_t*) {
  ++original_calls;ctx.r3.u64=original_result;
}
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main() {
  try {
    rex::InitLogging(nullptr,spdlog::level::warn);
    std::vector<uint8_t> memory(0x20000,0xA5);
    PPCContext ctx;ctx.r1.u64=0x10000;ctx.r4.u64=0x10064;ctx.lr=0x824785D4;
    Check(!rex::cvar::Query<bool>("iu_unlock_difficulties"),"unlock must default off");
    sub_82499008(ctx,memory.data());
    Check(original_calls==1 && ctx.r3.u64==0,"disabled option must preserve original locked result");
    Check(rex::cvar::SetFlagByName("iu_unlock_difficulties","true"),"enable live option");
    sub_82499008(ctx,memory.data());
    Check(original_calls==1 && ctx.r3.u64==1,"title selection must receive successful override");
    for(size_t i=0;i<memory.size();++i) {
      uint8_t expected=(i==0x10065 || i==0x10066)?1:(i==0x10064 || i==0x10067)?0:0xA5;
      Check(memory[i]==expected,"override must touch only the four-byte temporary output");
    }
    Check(ctx.r1.u64==0x10000 && ctx.r4.u64==0x10064 && ctx.lr==0x824785D4,"preserve call context");
    ctx.lr=0x82499554;sub_82499008(ctx,memory.data());
    Check(original_calls==2,"other profile queries must use original path");
    ctx.lr=0x824785D4;ctx.r4.u64++;
    sub_82499008(ctx,memory.data());Check(original_calls==3,"unexpected output contract must not patch memory");
    ctx.r4.u64--;sub_82499008(ctx,nullptr);
    Check(original_calls==4,"unreadable output must not patch memory");
    Check(rex::cvar::SetFlagByName("iu_unlock_difficulties","false"),"disable live option");
    original_result=1;sub_82499008(ctx,memory.data());
    Check(original_calls==5 && ctx.r3.u64==1,"turning off must preserve naturally unlocked results");
    std::cout<<"Difficulty selection override checks passed\n";
  } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
