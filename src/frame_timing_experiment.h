#pragma once

// Private investigation only; excluded from normal builds.
#include "guest_memory.h"
#include "frame_counter.h"
#include <chrono>
#include <bit>
#include <cstring>
#include <cstdio>
#include <mutex>

namespace iu::timing_experiment {
inline std::mutex log_mutex;
inline FILE* log_file() {
  static FILE* file = [] { FILE* result = nullptr; fopen_s(&result,"frame-timing.csv","w"); return result; }();
  return file;
}

extern "C" void __imp__sub_821B8240(PPCContext&, uint8_t*);
extern "C" void sub_821B8240(PPCContext& ctx, uint8_t* base) {
  const uint32_t timer = ctx.r3.u32;
  const bool update = uint32_t(ctx.lr) == 0x821B79AC && ctx.r5.s32 == 0;
  __imp__sub_821B8240(ctx, base);
  if (!update || !iu::guest_memory::readable(base, timer, 204)) return;
  const float delta = std::bit_cast<float>(iu::guest_memory::read32(base, timer+168));
  const float scale = std::bit_cast<float>(iu::guest_memory::read32(base, timer+192));
  std::lock_guard lock(log_mutex);
  static const auto start = std::chrono::steady_clock::now();
  static auto last = start;
  static unsigned updates = 0;
  static double simulated = 0;
  ++updates;
  simulated += double(delta)*scale;
  const auto now = std::chrono::steady_clock::now();
  if (now-last < std::chrono::seconds(1)) return;
  last = now;
  if (auto* file = log_file()) {
    static bool header = false;
    if (!header) {
      std::fputs("wall_seconds,updates,swaps,simulation_seconds,delta,scale,target_fps,vblank_steps\n",file);
      header = true;
    }
    std::fprintf(file,"%.6f,%u,%llu,%.6f,%.8f,%.3f,%u,%u\n",
      std::chrono::duration<double>(now-start).count(), updates,
      static_cast<unsigned long long>(iu::player::submitted_frames.load()), simulated,
      delta, scale, iu::guest_memory::read32(base,timer+196),
      unsigned(REX_LOAD_U16(timer+132)));
    std::fflush(file);
  }
}
} // namespace iu::timing_experiment
