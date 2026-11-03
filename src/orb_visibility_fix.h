#pragma once

#include <cmath>
#include "build_config.h"
#include "guest_memory.h"
#if IU_ENABLE_DIAGNOSTICS
#include "vesplume_diag.h"
#endif

namespace iu::orb_visibility {
// The sight point of Vesplume's patience pedestal is on its own collision
// surface. The native closed-segment query counts contact at t=1 as occlusion.
// Exclude that endpoint for this verified interaction, without changing the
// collision routines used by movement, combat, or other interaction targets.
inline bool shorten_endpoint(const float from[3], const float to[3], float result[3]) {
  for (unsigned i = 0; i < 3; ++i)
    if (!std::isfinite(from[i]) || !std::isfinite(to[i])) return false;
  bool changed = false;
  for (unsigned i = 0; i < 3; ++i) {
    result[i] = std::nextafter(to[i], from[i]);
    changed |= result[i] != to[i];
  }
  return changed;
}

class EndpointGuard {
 public:
  EndpointGuard(PPCContext& ctx, uint8_t* base) : base_(base) {
    // This exact call site passes its private stack vector at SP+80. Never
    // rewrite arbitrary caller data, shared geometry, or a different query.
    if (uint32_t(ctx.lr) != 0x8258F8B4 || ctx.r6.u32 != 0x100 ||
        uint64_t(ctx.r1.u32)+80 != ctx.r5.u32 ||
        !iu::guest_memory::readable(base, ctx.r9.u32, 3016) ||
        !iu::guest_memory::readable(base, ctx.r4.u32, 12) ||
        !iu::guest_memory::readable(base, ctx.r5.u32, 12)) return;
    const uint32_t object = ctx.r9.u32;
    if (iu::guest_memory::read32(base, object) != 0x8204EF34 ||
        iu::guest_memory::read32(base, object+632) != 10749 ||
        iu::guest_memory::read32(base, object+2804) != 38983) return;
    float from[3], to[3], shortened[3];
    for (unsigned i = 0; i < 3; ++i) {
      uint32_t bits = iu::guest_memory::read32(base, ctx.r4.u32+i*4);
      std::memcpy(&from[i], &bits, 4);
      bits = iu::guest_memory::read32(base, ctx.r5.u32+i*4);
      std::memcpy(&to[i], &bits, 4);
    }
    if (to[0] != -7800 || to[1] != 4100 || to[2] != 7200 ||
        !shorten_endpoint(from, to, shortened)) return;
    endpoint_ = iu::guest_memory::host_ptr(base_, ctx.r5.u32);
    std::memcpy(saved_, endpoint_, sizeof(saved_));
    for (unsigned i = 0; i < 3; ++i) {
      uint32_t bits;
      std::memcpy(&bits, &shortened[i], 4);
      bits = __builtin_bswap32(bits);
      std::memcpy(endpoint_+i*4, &bits, 4);
    }
    applied_ = true;
  }
  ~EndpointGuard() {
    if (applied_) std::memcpy(endpoint_, saved_, sizeof(saved_));
  }
  bool applied() const { return applied_; }
  EndpointGuard(const EndpointGuard&) = delete;
  EndpointGuard& operator=(const EndpointGuard&) = delete;
 private:
  uint8_t* base_;
  uint8_t* endpoint_ = nullptr;
  uint8_t saved_[12]{};
  bool applied_ = false;
};

extern "C" void __imp__sub_82520478(PPCContext&, uint8_t*);
extern "C" void sub_82520478(PPCContext& ctx, uint8_t* base) {
  EndpointGuard endpoint(ctx, base);
  __imp__sub_82520478(ctx, base);
#if IU_ENABLE_DIAGNOSTICS
  if (endpoint.applied() && iu::trace::enabled(iu::trace::kVesplume)) {
    static std::atomic<uint64_t> calls{0};
    const auto n = calls.fetch_add(1)+1;
    if (n <= 16 || n%256 == 0)
      vesp::emit("ORB_SIGHT_ENDPOINT_EXCLUDED CALL=%llu HIT=%u", n, ctx.r3.u32);
  }
#endif
}
} // namespace iu::orb_visibility
