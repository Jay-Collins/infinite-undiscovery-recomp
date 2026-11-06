#pragma once
#include <rex/cvar.h>
#include "guest_memory.h"

// CTitleTask queries four completion bytes into its stack at 824785D0.
// Its next branches offer Hard when byte 1 is set and Infinity when byte 2
// is set (824785DC..82478648). Override only this temporary query result:
// profile reads elsewhere, completion records, achievements and the selected
// difficulty of a running game retain the original behavior.
extern "C" void __imp__sub_82499008(PPCContext&, uint8_t*);
extern "C" void sub_82499008(PPCContext& ctx, uint8_t* base) {
  const uint32_t output=ctx.r4.u32;
  if (uint32_t(ctx.lr)==0x824785D4 &&
      uint64_t(ctx.r1.u32)+100==output &&
      rex::cvar::Query<bool>("iu_unlock_difficulties") &&
      iu::guest_memory::readable(base,output,4)) {
    REX_STORE_U8(output,0);
    REX_STORE_U8(output+1,1);
    REX_STORE_U8(output+2,1);
    REX_STORE_U8(output+3,0);
    ctx.r3.u64=1;
    return;
  }
  __imp__sub_82499008(ctx,base);
}
