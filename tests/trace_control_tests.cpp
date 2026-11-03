// Host-only regression tests. Guest originals are counters: no game assets or
// guest instructions are needed to verify trace gating and pass-through calls.
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
struct TestRegister { uint32_t u32 = 0; };
struct PPCContext {
  TestRegister r1, r3, r4, r5, r6, r7, r8, r9, r18, r23, r27, r30;
  struct { double f64 = 0; } f1;
  uint64_t lr = 0;
};
#include "../src/orb_interaction_diag.h"
#include "../src/orb_visibility_fix.h"
static int interaction_calls[13]{};
#define INTERACTION_STUB(address, index) \
  extern "C" void __imp__sub_##address(PPCContext& ctx, uint8_t*) { \
    ++interaction_calls[index]; ctx.r3.u32 = 0xA5000000u + index; \
  }
INTERACTION_STUB(8263D908, 0)
INTERACTION_STUB(82498588, 1)
INTERACTION_STUB(824DFFD0, 2)
INTERACTION_STUB(8258F798, 3)
INTERACTION_STUB(8263DBB0, 4)
INTERACTION_STUB(82498818, 5)
INTERACTION_STUB(824984F0, 6)
static bool probe_endpoint = false;
static float observed_endpoint[3]{};
extern "C" void __imp__sub_82520478(PPCContext& ctx, uint8_t* base) {
  ++interaction_calls[7];
  if (probe_endpoint)
    for (unsigned i = 0; i < 3; ++i)
      observed_endpoint[i] = iu::orb_diag::load_float(base, ctx.r5.u32+i*4);
  ctx.r3.u32 = 0xA5000007;
}
INTERACTION_STUB(82278498, 8)
INTERACTION_STUB(8242ECE8, 9)
INTERACTION_STUB(82426810, 10)
INTERACTION_STUB(82426860, 11)
INTERACTION_STUB(82429B78, 12)
#undef INTERACTION_STUB
static int writer_calls = 0, inventory_calls = 0;
static int move_calls = 0;
extern "C" void __imp__sub_8217DBB0(PPCContext& ctx, uint8_t* base) {
  ++move_calls;
  if (base) std::memcpy(iu::trace::host_ptr(base, ctx.r3.u32 + 856), iu::trace::host_ptr(base, ctx.r4.u32), 12);
}
extern "C" void __imp__sub_8219E8F0(PPCContext& ctx, uint8_t* base) {
  ++writer_calls;
  if (!base) return;
  const uint32_t persist = vesp::read32(base, vesp::kPersistPtrAddr);
  const uint32_t offset = (ctx.r4.u32 >> 5) * 4;
  const uint32_t bit = ctx.r4.u32 & 31;
  const uint32_t before = vesp::read32(base, persist + offset);
  const uint32_t after = (before & ~(1u << bit)) | ((ctx.r5.u32 & 255) << bit);
  const uint32_t be = __builtin_bswap32(after);
  std::memcpy(iu::trace::host_ptr(base, persist + offset), &be, 4);
}
extern "C" void __imp__sub_8247A960(PPCContext&, uint8_t*) { ++inventory_calls; }
static void Check(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
int main(int argc, char** argv) {
  try {
    if (argc != 2) return 2;
    const std::string mode = argv[1];
    const auto dir = std::filesystem::absolute("trace-test-" + mode);
    std::filesystem::create_directories(dir);
    _putenv_s("IU_TRACE", "");
    _putenv_s("IU_ORB_MOVE_TRACE", "1");
    _putenv_s("IU_ORB_ACTOR", "");
    _putenv_s("IU_ORB_FLAGS_ALL", mode == "flags" ? "1" : "");
    _putenv_s("IU_ORB_INTERACTION_TRACE", mode == "interaction" ? "1" : "");
    _putenv_s("IU_TRACE_DIR", dir.string().c_str());
    _putenv_s("IU_VESPLUME_TRACE_PATH", mode == "legacy" ? (dir / "legacy.log").string().c_str() : "");
    using namespace iu::trace;
    Check(!readable(nullptr, 0x10000), "Null guest base must not be treated as readable");
    // Model a guest word written with 1 << bit, then serialized big-endian.
    // Check every bit in two adjacent words, including the word boundary.
    for (uint32_t bit = 0; bit < 64; ++bit) {
      uint8_t bytes[8]{};
      const uint32_t word = 1u << (bit & 31);
      for (uint32_t byte = 0; byte < 4; ++byte)
        bytes[(bit / 32) * 4 + byte] = static_cast<uint8_t>(word >> (24 - 8 * byte));
      Check(bytes[vesp::flag_byte_offset(bit)] == (1u << (bit & 7)),
            "Flag trace must read the byte changed by the guest big-endian word writer");
    }
    if (mode == "visibility") {
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
      probe_endpoint=true;
      sub_82520478(ctx, memory);
      Check(interaction_calls[7] == 1 && ctx.r3.u32 == 0xA5000007 &&
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
    } else if (mode == "interaction") {
      using namespace iu::orb_interaction;
      using Wrapper = void (*)(PPCContext&, uint8_t*);
      const Wrapper wrappers[] = {sub_8263D908, sub_82498588, sub_824DFFD0, sub_8258F798,
        sub_8263DBB0, sub_82498818, sub_824984F0, iu::orb_visibility::sub_82520478, sub_82278498, sub_8242ECE8,
        sub_82426810, sub_82426860, sub_82429B78};
      auto memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0xE0020000ull, MEM_RESERVE, PAGE_NOACCESS));
      Check(memory != nullptr, "Reserve interaction arena");
      constexpr uint32_t object = 0xE0010000;
      Check(VirtualAlloc(host_ptr(memory, object), 4096, MEM_COMMIT, PAGE_READWRITE) != nullptr,
            "Commit physical interaction object");
      auto put = [&](uint32_t offset, uint32_t value) {
        value = __builtin_bswap32(value);
        std::memcpy(host_ptr(memory, object+offset), &value, 4);
      };
      put(0, kBoxVtable); put(632, kInstance); put(16, 0x12345678);
      Check(target(memory, object) && !target(nullptr, object), "Target must validate physical object reads");
      put(632, kInstance+1);
      Check(!target(memory, object), "Other authored instances must be excluded");
      put(632, kInstance);
      for (int pass = 0; pass < 3; ++pass) {
        if (pass == 1) StartRecording(kVesplume);
        if (pass == 2) StopRecording();
        const auto before = detail::ring().head.load();
        for (int i = 0; i < 13; ++i) {
          PPCContext ctx;
          ctx.r3.u32 = i >= 5 ? kState : object;
          ctx.r4.u32 = i == 1 ? kInstance : i == 5 ? 222 : i == 6 ? 581 : object;
          ctx.r5.u32 = 1; ctx.lr = 0x123456789ABCDEF0ull;
          const uint32_t arg4 = ctx.r4.u32;
          wrappers[i](ctx, memory);
          Check(interaction_calls[i] == pass+1 && ctx.r3.u32 == 0xA5000000u+i &&
                ctx.r4.u32 == arg4 && ctx.r5.u32 == 1 && ctx.lr == 0x123456789ABCDEF0ull,
                "Interaction observation must call original once and retain its result and context");
        }
        poll(memory);
        Check(pass == 1 ? detail::ring().head.load() > before : detail::ring().head.load() == before,
              "Interaction records must obey recorder start and stop");
      }
      Check(box.load() == object && box_handle.load() == 0x12345678,
            "Configuration must retain target identity for observations");
      VirtualFree(memory, 0, MEM_RELEASE);
    } else if (mode == "flags") {
      auto memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x82AA0000ull, MEM_RESERVE, PAGE_NOACCESS));
      Check(memory != nullptr, "Reserve persistence test arena");
      Check(VirtualAlloc(memory + 0x82A95000, 4096, MEM_COMMIT, PAGE_READWRITE) != nullptr,
            "Commit persistence pointer page");
      Check(VirtualAlloc(memory + 0x10000, vesp::kPersistBytes, MEM_COMMIT, PAGE_READWRITE) != nullptr,
            "Commit persistence data");
      const uint32_t pointer_be = __builtin_bswap32(0x10000u);
      std::memcpy(memory + vesp::kPersistPtrAddr, &pointer_be, 4);
      StartRecording(kVesplume);
      PPCContext ctx;
      ctx.r3.u32 = 0x1234; ctx.r4.u32 = 31; ctx.r5.u32 = 1; ctx.lr = 0x5678;
      const auto initial = detail::ring().head.load();
      vesp::sub_8219E8F0(ctx, memory);
      Check(writer_calls == 1 && vesp::read32(memory, 0x10000) == 0x80000000,
            "All-flags tracing must preserve an original write outside the old window");
      const auto changed = detail::ring().head.load();
      Check(changed > initial, "Changed flags must be logged");
      vesp::sub_8219E8F0(ctx, memory);
      Check(writer_calls == 2 && detail::ring().head.load() == changed,
            "Repeated unchanged writes must call original without logging");
      ctx.r5.u32 = 0;
      vesp::sub_8219E8F0(ctx, memory);
      Check(writer_calls == 3 && vesp::read32(memory, 0x10000) == 0 &&
            detail::ring().head.load() == changed + 1,
            "Clearing a flag must also be recorded");
      vesp::sub_8219E8F0(ctx, nullptr);
      Check(writer_calls == 4 && ctx.r3.u32 == 0x1234 && ctx.lr == 0x5678,
            "Unreadable diagnostic data must preserve original call and context");
      StopRecording();
      const auto stopped = detail::ring().head.load();
      ctx.r5.u32 = 1;
      vesp::sub_8219E8F0(ctx, memory);
      Check(writer_calls == 5 && vesp::read32(memory, 0x10000) == 0x80000000 &&
            detail::ring().head.load() == stopped, "Stopped flag tracing must pass through");
      VirtualFree(memory, 0, MEM_RELEASE);
    } else if (mode == "movement") {
      PPCContext ctx;
      ctx.r3.u32 = 0x10000; ctx.r4.u32 = 0x10400; ctx.lr = 0x12345678;
      iu::orb_diag::sub_8217DBB0(ctx, nullptr);
      Check(move_calls == 1 && !detail::ring().started.load(), "Disabled movement trace must pass through without logging");
      StartRecording(kVesplume);
      iu::orb_diag::sub_8217DBB0(ctx, nullptr);
      Check(move_calls == 2, "Unreadable movement arguments must still call original exactly once");
      auto memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x11000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
      Check(memory != nullptr, "Allocate synthetic guest memory");
      memory[ctx.r4.u32] = 0x3f; memory[ctx.r4.u32 + 1] = 0x80; // 1.0 big-endian
      const auto queued = detail::ring().head.load();
      iu::orb_diag::sub_8217DBB0(ctx, memory);
      Check(move_calls == 3 && std::memcmp(memory + 0x10000 + 856, memory + 0x10400, 12) == 0,
            "Movement observation must preserve the original operation");
      Check(detail::ring().head.load() > queued && ctx.lr == 0x12345678,
            "Movement trace must emit without changing guest context");
      VirtualFree(memory, 0, MEM_RELEASE);
      // Real actors use the 0xE... physical range. Reserve a sparse guest
      // arena and commit only its actor page to test the mapping end to end.
      auto sparse = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0xE0020000ull, MEM_RESERVE, PAGE_NOACCESS));
      Check(sparse != nullptr, "Reserve sparse guest arena");
      ctx.r3.u32 = 0xE0010000; ctx.r4.u32 = 0xE0010400;
      Check(VirtualAlloc(host_ptr(sparse, ctx.r3.u32), 4096, MEM_COMMIT, PAGE_READWRITE) != nullptr,
            "Commit physical actor page");
      auto vec = host_ptr(sparse, ctx.r4.u32);
      vec[0] = 0x3f; vec[1] = 0x80;
      iu::orb_diag::sub_8217DBB0(ctx, sparse);
      Check(move_calls == 4 && iu::orb_diag::load_float(sparse, ctx.r3.u32 + 856) == 1.0f &&
            vesp::read8(sparse, ctx.r4.u32) == 0x3f,
            "Physical actor observation must use the generated code's address bias");
      VirtualFree(sparse, 0, MEM_RELEASE);
      StopRecording();
    } else if (mode == "start") {
      Check(!vesp::trace_path(), "Tracing must initially be off");
      StartRecording(kVesplume);
      Check(vesp::trace_path(), "Starting after the first disabled call must enable Vesplume");
      AddMarker("orb-handoff");
      Check(detail::ring().head.load() == 2, "Vesplume-only session must retain start and marker metadata");
      StopRecording();
      Check(!vesp::trace_path(), "Stop must disable Vesplume");
      Check(detail::ring().head.load() == 3, "Stop metadata must survive filtering");
      StartRecording(kVesplume);
      Check(vesp::trace_path(), "Restart must re-enable Vesplume");
      StopRecording();
    } else if (mode == "stop" || mode == "legacy") {
      if (mode == "stop") StartRecording(kVesplume);
      Check(vesp::trace_path(), "Vesplume should be enabled");
      PPCContext ctx;
      ctx.r3.u32 = 0x1234; ctx.lr = 0x5678;
      vesp::sub_8219E8F0(ctx, nullptr); // bit zero: no diagnostic guest reads
      vesp::sub_8247A960(ctx, nullptr);
      StopRecording();
      Check(!vesp::trace_path(), "Stop must also override the legacy launch setting");
      const auto queued = detail::ring().head.load();
      vesp::sub_8219E8F0(ctx, nullptr);
      vesp::sub_8247A960(ctx, nullptr);
      Check(writer_calls == 2 && inventory_calls == 2, "Each wrapper must call the original exactly once");
      Check(ctx.r3.u32 == 0x1234 && ctx.lr == 0x5678, "Tracing must preserve context");
      Check(detail::ring().head.load() == queued, "Stopped wrappers must not enqueue records");
    } else if (mode == "filter") {
      emit(kSavePoint, "must not be emitted");
      Check(!detail::ring().started.load(), "Disabled emission must not start a logging thread");
      Check(detail::ring().head.load() == 0, "Disabled emission must not enqueue records");
      StartRecording(kVesplume);
      const auto queued = detail::ring().head.load();
      emit(kSavePoint, "wrong category");
      Check(detail::ring().head.load() == queued, "Direct calls must obey category filtering");
      emit(kVesplume, "selected category");
      Check(detail::ring().head.load() == queued + 1, "Selected category must emit");
      StopRecording();
    } else return 2;
    std::cout << "PASS " << mode << '\n';
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
