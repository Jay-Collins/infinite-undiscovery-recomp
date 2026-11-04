#pragma once
#include "menu_pause.h"

// The title finishes its frame work and calculates the next simulation delta
// here, including the existing 30/60 FPS vblank wait. Park after that work so
// it can complete normally even if F4 was opened mid-frame. No guest registers
// or saved results are changed by the host wait.
extern "C" void __imp__sub_821B7948(PPCContext&, uint8_t*);
extern "C" void sub_821B7948(PPCContext& ctx, uint8_t* base) {
  __imp__sub_821B7948(ctx, base);
  iu::player::menu_pause.FrameBoundary();
}

// This title callback increments 0x82A58408 and signals the frame timer event.
// Suppress it only once the frame is parked (never merely on a pause request,
// which would strand the frame's own vblank wait). SDK vblanks and host UI
// rendering keep running. The simulation sees no menu-time catch-up on resume.
// A callback already in flight may finish, adding at most one normal tick.
extern "C" void __imp__sub_821FC010(PPCContext&, uint8_t*);
extern "C" void sub_821FC010(PPCContext& ctx, uint8_t* base) {
  if (!iu::player::menu_pause.IsPaused()) __imp__sub_821FC010(ctx, base);
}
