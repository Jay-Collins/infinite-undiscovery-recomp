#pragma once

// Opt-in, read-only observation of the Orb of Patience interaction. Addresses
// are from the supplied USA executable; see docs/ORB-INVESTIGATION.md.
#include "orb_move_diag.h"
#include <cmath>

namespace iu::orb_interaction {
inline constexpr uint32_t kInstance = 10749, kBoxVtable = 0x8204EF34;
inline constexpr uint32_t kState = 0x82A5935C;
inline std::atomic<uint32_t> box{0}, box_handle{0};
inline bool requested() {
  static const bool value = [] {
    const char* env = std::getenv("IU_ORB_INTERACTION_TRACE");
    return env && std::strcmp(env, "1") == 0;
  }();
  return value && trace::enabled(trace::kVesplume);
}
inline bool target(uint8_t* base, uint32_t object) {
  return trace::readable(base, object, 3016) &&
         vesp::read32(base, object) == kBoxVtable &&
         vesp::read32(base, object + 632) == kInstance;
}
inline bool sample(std::atomic<uint64_t>& count) {
  const auto n = count.fetch_add(1) + 1;
  return n <= 16 || n % 256 == 0;
}
inline void describe(uint8_t* base, uint32_t object, const char* stage) {
  if (!target(base, object)) return;
  using orb_diag::load_float;
  vesp::emit("ORB_BOX %s OBJ=%08X HANDLE=%08X INSTANCE=%u SCRIPT=%08X POS=(%.5g,%.5g,%.5g) CENTER=(%.5g,%.5g,%.5g) HALF=(%.5g,%.5g,%.5g) F36=%02X F588=%u F652=%08X F722=%u F727=%u",
    stage, object, vesp::read32(base, object+16), kInstance,
    vesp::read32(base, object+2804), load_float(base, object+856),
    load_float(base, object+860), load_float(base, object+864),
    load_float(base, object+2952), load_float(base, object+2956), load_float(base, object+2960),
    load_float(base, object+2964), load_float(base, object+2968), load_float(base, object+2972),
    vesp::read8(base, object+36), vesp::read8(base, object+588), vesp::read32(base, object+652),
    vesp::read8(base, object+722), vesp::read8(base, object+727));
}

// Called at an existing guest fiber boundary. No guest calls or guest writes.
inline void poll(uint8_t* base) {
  if (!requested()) return;
  static std::atomic<uint64_t> next{0};
  const uint64_t now = GetTickCount64();
  auto previous = next.load();
  if (now < previous || !next.compare_exchange_strong(previous, now + 2000)) return;
  const uint32_t object = box.load();
  if (!target(base, object) || vesp::read32(base, object+16) != box_handle.load()) return;
  describe(base, object, "POLL");
  uint32_t state = 0, door = 0;
  const bool state_ok = trace::peek32(base, kState+576+222*8+4, state);
  const bool door_ok = trace::peek32(base, kState+(581/32)*4, door);
  vesp::emit("ORB_STATE V222=%u VALID=%u S581=%u VALID=%u", state, state_ok,
             (door >> (581%32)) & 1, door_ok);
  // Read the same generation-checked actor slot used by sub_82147140.
  uint32_t handle = 0, table = 0, player = 0;
  if (!trace::peek32(base, 0x82A58EA8+120, handle) || !handle ||
      !trace::peek32(base, 0x82A380F8+24, table) || !table) return;
  const uint64_t slot = uint64_t(table)+180+(handle&0xFFFF)*4;
  if (slot > UINT32_MAX || !trace::peek32(base, uint32_t(slot), player) ||
      !trace::readable(base, player, 868) ||
      vesp::read32(base, player+16) != handle ||
      !(vesp::read32(base, player+28) & (1u<<19))) return;
  vesp::emit("ORB_PLAYER OBJ=%08X HANDLE=%08X POS=(%.5g,%.5g,%.5g)",
    player, handle, orb_diag::load_float(base, player+856),
    orb_diag::load_float(base, player+860), orb_diag::load_float(base, player+864));
}

extern "C" void __imp__sub_8263D908(PPCContext&, uint8_t*);
extern "C" void sub_8263D908(PPCContext& ctx, uint8_t* base) {
  const uint32_t object = ctx.r3.u32;
  __imp__sub_8263D908(ctx, base);
  if (requested() && target(base, object)) {
    box_handle.store(vesp::read32(base, object+16));
    box.store(object);
    describe(base, object, "CONFIGURED");
  }
}

extern "C" void __imp__sub_82498588(PPCContext&, uint8_t*);
extern "C" void sub_82498588(PPCContext& ctx, uint8_t* base) {
  const bool log = requested() && ctx.r4.u32 == kInstance;
  const uint32_t lr = uint32_t(ctx.lr);
  __imp__sub_82498588(ctx, base);
  if (log) vesp::emit("ORB_SUPPRESSION_CHECK RESULT=%u LR=%08X", ctx.r3.u32, lr);
}

extern "C" void __imp__sub_824DFFD0(PPCContext&, uint8_t*);
extern "C" void sub_824DFFD0(PPCContext& ctx, uint8_t* base) {
  static std::atomic<uint64_t> calls{0};
  const bool log = requested() && target(base, ctx.r3.u32) && sample(calls);
  const uint32_t object = ctx.r3.u32, vector = ctx.r4.u32, lr = uint32_t(ctx.lr);
  __imp__sub_824DFFD0(ctx, base);
  if (log) {
    vesp::emit("ORB_FACING CALL=%llu OBJ=%08X RESULT=%u LR=%08X", calls.load(), object, ctx.r3.u32, lr);
    if (trace::readable(base, vector, 12))
      vesp::emit("ORB_FACING_POINT POS=(%.5g,%.5g,%.5g)", orb_diag::load_float(base, vector),
                 orb_diag::load_float(base, vector+4), orb_diag::load_float(base, vector+8));
  }
}

extern "C" void __imp__sub_8258F798(PPCContext&, uint8_t*);
extern "C" void sub_8258F798(PPCContext& ctx, uint8_t* base) {
  static std::atomic<uint64_t> calls{0};
  const bool log = requested() && target(base, ctx.r4.u32) && sample(calls);
  const uint32_t object = ctx.r4.u32, lr = uint32_t(ctx.lr);
  __imp__sub_8258F798(ctx, base);
  if (log) vesp::emit("ORB_SIGHT CALL=%llu OBJ=%08X RESULT=%u LR=%08X", calls.load(), object, ctx.r3.u32, lr);
}

inline void observe_interaction_request(const PPCContext& ctx) {
  if (requested()) vesp::emit("ORB_INTERACT_REQUEST TARGET_HANDLE=%08X MODE=%u LR=%08X",
                             ctx.r4.u32, ctx.r5.u32, uint32_t(ctx.lr));
}

inline void vector_record(uint8_t* base, const char* name, uint32_t vector) {
  if (trace::readable(base, vector, 12))
    vesp::emit("ORB_VECTOR %s=(%.9g,%.9g,%.9g)", name, orb_diag::load_float(base, vector),
      orb_diag::load_float(base, vector+4), orb_diag::load_float(base, vector+8));
}

extern "C" void __imp__sub_82278498(PPCContext&, uint8_t*);
extern "C" void sub_82278498(PPCContext& ctx, uint8_t* base) {
  static std::atomic<uint64_t> calls{0};
  const bool log = requested() && uint32_t(ctx.lr) == 0x82520724 &&
                   target(base, ctx.r18.u32) && sample(calls);
  const uint32_t collider = ctx.r3.u32, point = ctx.r6.u32, triangle = ctx.r8.u32;
  __imp__sub_82278498(ctx, base);
  if (log) {
    vesp::emit("ORB_MAP_RAY CALL=%llu COLLIDER=%08X HIT=%u", calls.load(), collider, ctx.r3.u32);
    if (ctx.r3.u32 & 255) {
      vector_record(base, "MAP_HIT", point);
      vector_record(base, "TRI_A", triangle);
      vector_record(base, "TRI_B", triangle+12);
      vector_record(base, "TRI_C", triangle+24);
      vector_record(base, "TRI_NORMAL", triangle+36);
    }
  }
}

extern "C" void __imp__sub_8242ECE8(PPCContext&, uint8_t*);
extern "C" void sub_8242ECE8(PPCContext& ctx, uint8_t* base) {
  static std::atomic<uint64_t> calls{0};
  const bool log = requested() && uint32_t(ctx.lr) == 0x826DFCFC &&
                   target(base, ctx.r23.u32) && sample(calls);
  const uint32_t object = ctx.r30.u32, collider = ctx.r3.u32;
  __imp__sub_8242ECE8(ctx, base);
  if (log && trace::readable(base, object, 868)) {
    vesp::emit("ORB_OBJECT_RAY CALL=%llu OBJ=%08X VT=%08X HANDLE=%08X INSTANCE=%u COLLIDER=%08X HIT=%u",
      calls.load(), object, vesp::read32(base, object), vesp::read32(base, object+16),
      vesp::read32(base, object+632), collider, ctx.r3.u32);
    // CMapAreaObject is not an actor: +856 is not a position for this class.
  }
}

inline bool pedestal_ray(uint8_t* base, uint32_t ray) {
  if (!trace::readable(base, ray, 32)) return false;
  constexpr float end[3] = {-7800, 4100, 7200};
  for (uint32_t i = 0; i < 3; ++i) {
    const float value = orb_diag::load_float(base, ray+i*4) + orb_diag::load_float(base, ray+16+i*4);
    if (!(std::abs(value-end[i]) < 0.25f)) return false;
  }
  return true;
}
inline void shape_record(uint8_t* base, const char* name, uint32_t shape, uint32_t quads) {
  if (!trace::readable(base, shape, quads*16)) return;
  for (uint32_t i = 0; i < quads; ++i)
    vesp::emit("ORB_SHAPE %s Q%u=(%.9g,%.9g,%.9g,%.9g)", name, i,
      orb_diag::load_float(base, shape+i*16), orb_diag::load_float(base, shape+i*16+4),
      orb_diag::load_float(base, shape+i*16+8), orb_diag::load_float(base, shape+i*16+12));
}
extern "C" void __imp__sub_82426810(PPCContext&, uint8_t*);
extern "C" void sub_82426810(PPCContext& ctx, uint8_t* base) {
  static std::atomic<uint64_t> calls{0};
  const bool log = requested() && pedestal_ray(base, ctx.r4.u32) && sample(calls);
  const uint32_t shape = ctx.r3.u32, ray = ctx.r4.u32;
  const double radius = ctx.f1.f64;
  __imp__sub_82426810(ctx, base);
  if (log) {
    vesp::emit("ORB_OBB HIT=%u DIST2=%.17g RADIUS=%.17g", ctx.r3.u32, ctx.f1.f64, radius);
    shape_record(base, "OBB", shape, 5); shape_record(base, "RAY", ray, 2);
  }
}
extern "C" void __imp__sub_82426860(PPCContext&, uint8_t*);
extern "C" void sub_82426860(PPCContext& ctx, uint8_t* base) {
  static std::atomic<uint64_t> calls{0};
  const bool log = requested() && pedestal_ray(base, ctx.r4.u32) && sample(calls);
  const uint32_t shape = ctx.r3.u32, ray = ctx.r4.u32;
  const double radius = ctx.f1.f64;
  __imp__sub_82426860(ctx, base);
  if (log) {
    vesp::emit("ORB_SPHERE HIT=%u DIST2=%.17g EXTRA_RADIUS=%.17g", ctx.r3.u32, ctx.f1.f64, radius);
    shape_record(base, "SPHERE", shape, 1); shape_record(base, "RAY", ray, 2);
  }
}
extern "C" void __imp__sub_82429B78(PPCContext&, uint8_t*);
extern "C" void sub_82429B78(PPCContext& ctx, uint8_t* base) {
  static std::atomic<uint64_t> calls{0};
  const bool log = requested() && uint32_t(ctx.lr) == 0x8242B480 &&
                   pedestal_ray(base, ctx.r27.u32) && sample(calls);
  const uint32_t shape = ctx.r4.u32, matrix = ctx.r5.u32, output = ctx.r3.u32;
  __imp__sub_82429B78(ctx, base);
  if (log) {
    shape_record(base, "LOCAL_OBB", shape, 2); shape_record(base, "MATRIX", matrix, 3);
    shape_record(base, "WORLD_OBB", output, 5);
  }
}

extern "C" void __imp__sub_8263DBB0(PPCContext&, uint8_t*);
extern "C" void sub_8263DBB0(PPCContext& ctx, uint8_t* base) {
  if (requested() && target(base, ctx.r3.u32)) {
    describe(base, ctx.r3.u32, "ACTIVATED");
    vesp::emit("ORB_ACTIVATE ACTOR_HANDLE=%08X LR=%08X", ctx.r4.u32, uint32_t(ctx.lr));
  }
  __imp__sub_8263DBB0(ctx, base);
}

extern "C" void __imp__sub_82498818(PPCContext&, uint8_t*);
extern "C" void sub_82498818(PPCContext& ctx, uint8_t* base) {
  const uint32_t state = ctx.r3.u32;
  const bool log = requested() && state == kState && ctx.r4.u32 == 222;
  __imp__sub_82498818(ctx, base);
  if (log && trace::readable(base, state+576+222*8, 8))
    vesp::emit("ORB_V222_WRITE TYPE=%08X VALUE=%u", vesp::read32(base, state+576+222*8),
               vesp::read32(base, state+576+222*8+4));
}

extern "C" void __imp__sub_824984F0(PPCContext&, uint8_t*);
extern "C" void sub_824984F0(PPCContext& ctx, uint8_t* base) {
  const bool log = requested() && ctx.r3.u32 == kState && ctx.r4.u32 == 581;
  const uint32_t value = ctx.r5.u32;
  __imp__sub_824984F0(ctx, base);
  if (log) vesp::emit("ORB_S581_WRITE VALUE=%u", value);
}
} // namespace iu::orb_interaction
