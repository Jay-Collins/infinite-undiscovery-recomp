#pragma once
#include "frame_counter.h"
#include <windows.h>
#include <cstdlib>

// Count actual guest swap submissions rather than ImGui repaint events.
// This is a lightweight player FPS counter, with no recording or debug UI.
extern "C" void __imp__VdSwap(PPCContext& ctx, uint8_t* base) {
  static PPCFunc* const original = []() -> PPCFunc* {
    auto* proc=GetProcAddress(GetModuleHandleW(L"rexruntime.dll"),"__imp__VdSwap");
    if (!proc) std::abort();
    return reinterpret_cast<PPCFunc*>(proc);
  }();
  original(ctx,base);
  iu::player::submitted_frames.fetch_add(1,std::memory_order_relaxed);
}
