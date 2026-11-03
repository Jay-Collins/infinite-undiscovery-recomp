// Verify the production orb hook without including diagnostic headers.
#include <cstdint>
#include <iostream>
#include <stdexcept>
struct TestRegister { uint32_t u32 = 0; };
struct PPCContext { TestRegister r1, r3, r4, r5, r6, r9; uint64_t lr = 0; };
#include "../src/orb_visibility_fix.h"
static_assert(IU_ENABLE_DIAGNOSTICS == 0);
static int calls = 0;
static float observed_endpoint[3]{};
extern "C" void __imp__sub_82520478(PPCContext& ctx, uint8_t* base) {
  ++calls;
  for (unsigned i=0; i<3; ++i) {
    const auto bits=iu::guest_memory::read32(base,ctx.r5.u32+i*4);
    std::memcpy(&observed_endpoint[i],&bits,4);
  }
  ctx.r3.u32=0xA5000007;
}
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
  try {
    using namespace iu::guest_memory;
      using namespace iu::orb_visibility;
      auto memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0xE0020000ull, MEM_RESERVE, PAGE_NOACCESS));
      Check(memory != nullptr, "Reserve sight test arena");
      constexpr uint32_t object = 0xE0010000, from = object+0xC00, to = object+0xC10;
      Check(VirtualAlloc(host_ptr(memory, object), 4096, MEM_COMMIT, PAGE_READWRITE) != nullptr,
            "Commit sight test page");
      auto put = [&](uint32_t address, uint32_t value) {
        value = __builtin_bswap32(value); std::memcpy(host_ptr(memory, address), &value, 4);
      };
      auto put_float = [&](uint32_t address, float value) {
        uint32_t bits; std::memcpy(&bits, &value, 4); put(address, bits);
      };
      put(object, 0x8204EF34); put(object+632, 10749); put(object+2804, 38983);
      const float origin[] = {-7738.7583f,4159.52051f,7205.27539f};
      const float endpoint[] = {-7800,4100,7200};
      for (unsigned i = 0; i < 3; ++i) { put_float(from+i*4, origin[i]); put_float(to+i*4, endpoint[i]); }
      uint8_t saved[12]; std::memcpy(saved, host_ptr(memory, to), 12);
      PPCContext ctx;
      ctx.r1.u32=to-80; ctx.r4.u32=from; ctx.r5.u32=to; ctx.r6.u32=0x100;
      ctx.r9.u32=object; ctx.lr=0x8258F8B4;
      sub_82520478(ctx, memory);
      Check(calls == 1 && ctx.r3.u32 == 0xA5000007 &&
            ctx.r1.u32 == to-80 && ctx.r5.u32 == to && ctx.lr == 0x8258F8B4,
            "Sight correction must preserve call count, result, registers and stack");
      Check(observed_endpoint[0] > endpoint[0] && observed_endpoint[1] > endpoint[1] &&
            observed_endpoint[2] > endpoint[2] && std::memcmp(saved, host_ptr(memory,to),12) == 0,
            "Native query must see the shortened endpoint and caller vector must be restored");
      // Each guard independently excludes a different query/target.
      for (int guard=0; guard<5; ++guard) {
        if (guard==0) ctx.lr=0x1234;
        if (guard==1) ctx.r1.u32=to-96;
        if (guard==2) ctx.r6.u32=1;
        if (guard==3) put(object+632,10750);
        if (guard==4) put(object+2804,1);
        sub_82520478(ctx,memory);
        Check(std::memcmp(observed_endpoint,endpoint,12)==0 &&
              std::memcmp(saved,host_ptr(memory,to),12)==0, "Unrelated query must remain byte-identical");
        ctx.lr=0x8258F8B4; ctx.r1.u32=to-80; ctx.r6.u32=0x100;
        put(object+632,10749); put(object+2804,38983);
      }
      try { EndpointGuard guard(ctx,memory); Check(guard.applied(),"Expected temporary endpoint"); throw 1; }
      catch (int) {}
      Check(std::memcmp(saved,host_ptr(memory,to),12)==0,"Unwinding must restore private endpoint bytes");
      float result[3];
      Check(!shorten_endpoint(endpoint,endpoint,result),"Zero-length rays must be unchanged");
      const float invalid[] = {NAN,0,0};
      Check(!shorten_endpoint(invalid,endpoint,result),"Non-finite rays must be unchanged");
      VirtualFree(memory,0,MEM_RELEASE);
    std::cout << "Production orb hook: passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
  }
}
