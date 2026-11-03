#pragma once

// Opt-in observation of the position setter used by SafeStep. This is a
// candidate trace point, not a claim that all scripted movement uses it.
// Enable with IU_TRACE=VESPLUME and IU_ORB_MOVE_TRACE=1. Optionally restrict
// to one object with IU_ORB_ACTOR=0x.... No guest state is changed here.
#include <cstring>
#include "vesplume_diag.h"

namespace iu::orb_diag {
inline bool requested() {
  static const bool value = [] {
    const char* env = std::getenv("IU_ORB_MOVE_TRACE");
    return env && std::strcmp(env, "1") == 0;
  }();
  return value && trace::enabled(trace::kVesplume);
}
inline uint32_t actor_filter() {
  static const uint32_t value = [] {
    const char* env = std::getenv("IU_ORB_ACTOR");
    return env ? static_cast<uint32_t>(std::strtoul(env, nullptr, 0)) : 0u;
  }();
  return value;
}
inline float load_float(uint8_t* base, uint32_t address) {
  const uint32_t bits = vesp::read32(base, address);
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}
inline std::atomic<uint64_t> calls{0};
extern "C" void __imp__sub_8217DBB0(PPCContext&, uint8_t*);
extern "C" void sub_8217DBB0(PPCContext& ctx, uint8_t* base) {
  if (!requested()) {
    __imp__sub_8217DBB0(ctx, base);
    return;
  }
  const uint32_t actor = ctx.r3.u32, vector = ctx.r4.u32;
  if ((actor_filter() && actor != actor_filter()) ||
      !trace::readable(base, actor, 868) || !trace::readable(base, vector, 12)) {
    __imp__sub_8217DBB0(ctx, base);
    return;
  }
  const uint64_t call = calls.fetch_add(1) + 1;
  // Bound the cost and log size. Later calls remain sampled, including a
  // count, so absence of an entry cannot be mistaken for no movement.
  const bool sample = call <= 4096 || call % 1024 == 0;
  if (sample) {
    vesp::emit("MOVE_ENTER CALL=%llu ACTOR=0x%08X ID16=0x%08X VT=0x%08X TYPES_LO=0x%08X LR=0x%08X POS=(%.5g,%.5g,%.5g) REQUEST=(%.5g,%.5g,%.5g) F722=%u F727=%u",
      static_cast<unsigned long long>(call), actor, vesp::read32(base, actor + 16),
      vesp::read32(base, actor), vesp::read32(base, actor + 28),
      static_cast<uint32_t>(ctx.lr), load_float(base, actor + 856),
      load_float(base, actor + 860), load_float(base, actor + 864),
      load_float(base, vector), load_float(base, vector + 4), load_float(base, vector + 8),
      vesp::read8(base, actor + 722), vesp::read8(base, actor + 727));
  }
  __imp__sub_8217DBB0(ctx, base);
  if (sample && trace::readable(base, actor, 868)) {
    vesp::emit("MOVE_AFTER CALL=%llu ACTOR=0x%08X POS=(%.5g,%.5g,%.5g) F722=%u F727=%u",
      static_cast<unsigned long long>(call), actor, load_float(base, actor + 856),
      load_float(base, actor + 860), load_float(base, actor + 864),
      vesp::read8(base, actor + 722), vesp::read8(base, actor + 727));
  }
}
}  // namespace iu::orb_diag
