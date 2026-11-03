#pragma once
#include <rex/cvar.h>

// The title selects 30 FPS at this initialization call. Its existing scheduler
// (821B8240) waits for 60 / target vblanks and derives delta time from elapsed
// vblanks. Select 60 here without changing the video clock or simulation scale.
// Only this verified call is overridden; movie playback and other requests
// retain their original timing. The setting takes effect on the next launch.
extern "C" void __imp__sub_826CE9C8(PPCContext&, uint8_t*);
extern "C" void sub_826CE9C8(PPCContext& ctx, uint8_t* base) {
  if (uint32_t(ctx.lr) == 0x821CB6B8 && ctx.r4.u32 == 30 &&
      rex::cvar::GetFlagByName("iu_frame_rate") == "60")
    ctx.r4.u64 = 60;
  __imp__sub_826CE9C8(ctx, base);
}
