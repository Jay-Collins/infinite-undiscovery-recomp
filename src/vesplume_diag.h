#pragma once

// Vesplume Phase 4 diagnostic instrumentation (developer diagnostic build).
//
// Overrides two guest functions using ReXGlue's weak-symbol alias mechanism:
// DEFINE_REX_FUNC(sub_<addr>) emits the original body as __imp__sub_<addr> and
// makes sub_<addr> a weak alias of it. Defining a strong sub_<addr> here wins
// at link time; the original body stays callable as __imp__sub_<addr> and is
// invoked exactly once by each wrapper.
//
// Included once from src/main.cpp, AFTER generated/default/infinite_undiscovery_init.h
// (which supplies PPCContext, the sub_* declarations, and the guest memory model).
//
// This is purely observational. It does not modify save logic, force flags, or
// alter guest registers/state beyond the wrapper call itself. When the
// VESPLUME category is disabled the wrappers are a
// straight pass-through to the original guest body.

#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

#include "iu_trace.h"

struct PPCContext;

namespace vesp {

// ---- Logging -------------------------------------------------------------
// Controlled by IU_VESPLUME_TRACE_PATH (legacy) or IU_TRACE=VESPLUME (common framework).
// InfiniteUndiscoveryApp::SetupEnvironment redirects the legacy variable to
// <portable>/logs/vesplume_trace.log when it exists. Output now goes through the common
// non-blocking sink (iu_trace.h); file name, header and line format are unchanged.
inline const char* trace_path() {
  if (!iu::trace::enabled(iu::trace::kVesplume)) return nullptr;
  static const std::string path = []() -> std::string {
    const char* env = std::getenv("IU_VESPLUME_TRACE_PATH");
    if (env && env[0] != '\0') return env;
    return iu::trace::log_dir() + "\\vesplume_trace.log";
  }();
  return path.c_str();
}

inline std::atomic<unsigned long long> g_seq{0};
inline std::atomic<bool> g_header{false};

inline void emit(const char* fmt, ...) {
  if (!trace_path()) return;
  using iu::trace::emit_line_to;
  if (!g_header.exchange(true)) {
    emit_line_to(iu::trace::kDestVesplume, "IU RECOMP VESPLUME DIAGNOSTIC TRACE");
    emit_line_to(iu::trace::kDestVesplume, "Phase 4 Developer Build v1 PHYSICAL_HOST_BIAS=0x1000");
    emit_line_to(iu::trace::kDestVesplume,
                 "PERSIST_PTR_ADDR=0x82A95A20 PERSIST_WINDOW_BYTES=9380..9575 BIT_RANGE=75056..76607");
  }
  char body[400];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(body, sizeof(body), fmt, ap);
  va_end(ap);
  char line[480];
  const unsigned long long seq = g_seq.fetch_add(1) + 1;
  std::snprintf(line, sizeof(line), "SEQ %06llu TICK %llu TID %lu %s", seq,
                static_cast<unsigned long long>(GetTickCount64()),
                static_cast<unsigned long>(GetCurrentThreadId()), body);
  emit_line_to(iu::trace::kDestVesplume, line);
}

// Guest-memory readers, including the +0x1000 bias for physical addresses.
// Guest words are big-endian in host RAM.
inline uint8_t read8(uint8_t* base, uint32_t addr) {
  return *(volatile uint8_t*)iu::trace::host_ptr(base, addr);
}
inline uint32_t read32(uint8_t* base, uint32_t addr) {
  return __builtin_bswap32(*(volatile uint32_t*)iu::trace::host_ptr(base, addr));
}

// ---- Phase 3 established facts -------------------------------------------
inline constexpr uint32_t kPersistPtrAddr = 0x82A95A20u;  // -> 102400-byte persistence array
inline constexpr uint32_t kBitLo = 75056u;
inline constexpr uint32_t kBitHi = 76607u;
inline constexpr uint32_t kPersistBytes = 102400u;
inline bool all_flag_changes() {
  static const bool requested = [] {
    const char* env = std::getenv("IU_ORB_FLAGS_ALL");
    return env && std::strcmp(env, "1") == 0;
  }();
  return requested;
}
// The writer indexes low bits of a big-endian 32-bit word, so byte order
// within each word is reversed relative to bit / 8.
inline constexpr uint32_t flag_byte_offset(uint32_t bit) {
  return (bit >> 5) * 4 + 3 - ((bit & 31) >> 3);
}

// ---- PRIMARY: persistence single-bit writer 0x8219E8F0 -------------------
extern "C" void __imp__sub_8219E8F0(PPCContext& ctx, uint8_t* base);

inline std::atomic<unsigned long long> g_writer_total{0};
inline std::atomic<unsigned long long> g_writer_in_window{0};
inline std::atomic<unsigned long long> g_writer_other{0};

extern "C" void sub_8219E8F0(PPCContext& ctx, uint8_t* base) {
  if (!trace_path()) {  // disabled: pure pass-through
    __imp__sub_8219E8F0(ctx, base);
    return;
  }

  const unsigned long long call_no = g_writer_total.fetch_add(1) + 1;
  const uint32_t bit = ctx.r4.u32;    // bit index
  const uint32_t value = ctx.r5.u32;  // value byte
  const uint32_t r3 = ctx.r3.u32;     // context
  const uint32_t lr = static_cast<uint32_t>(ctx.lr);
  const uint32_t byte_off = flag_byte_offset(bit);
  const bool in_window = (bit >= kBitLo && bit <= kBitHi);

  // The original narrow window has not been established as the orb's quest
  // state. Opt-in coverage observes all valid persistence words, logging only
  // actual changes so repeated map/visibility writes do not drown them out.
  if (all_flag_changes()) {
    const uint32_t word_off = (bit >> 5) * 4;
    uint32_t persist = 0;
    if (word_off < kPersistBytes && iu::trace::readable(base, kPersistPtrAddr, 4))
      persist = read32(base, kPersistPtrAddr);
    const bool valid = persist != 0 &&
        static_cast<uint64_t>(persist) + word_off + 4 <= 0x100000000ull &&
        iu::trace::readable(base, persist + word_off, 4);
    const uint32_t before = valid ? read32(base, persist + word_off) : 0;
    __imp__sub_8219E8F0(ctx, base);
    if (valid && iu::trace::readable(base, persist + word_off, 4)) {
      const uint32_t after = read32(base, persist + word_off);
      if (before != after)
        emit("FLAG_CHANGE CALL=%llu LR=0x%08X R3=0x%08X BIT=%u VALUE=%u BASE=0x%08X WORD=%u BEFORE=0x%08X AFTER=0x%08X",
             call_no, lr, r3, bit, value, persist, word_off, before, after);
    }
    return;
  }

  if (!in_window) {
    // Bounded summary: prove the writer ran for unrelated indices without spam.
    const unsigned long long other = g_writer_other.fetch_add(1) + 1;
    if (other <= 8 || (other % 8192) == 0) {
      emit("WRITER_NONWINDOW FUNC=0x8219E8F0 LR=0x%08X R3=0x%08X BIT=%u VALUE=%u CALL=%llu OTHER=%llu",
           lr, r3, bit, value, call_no, other);
    }
    __imp__sub_8219E8F0(ctx, base);
    return;
  }

  const uint32_t word_off = byte_off & ~3u;
  const uint32_t persist = iu::trace::readable(base, kPersistPtrAddr, 4)
      ? read32(base, kPersistPtrAddr) : 0;
  if (!persist || static_cast<uint64_t>(persist) + word_off + 4 > 0x100000000ull ||
      !iu::trace::readable(base, persist + word_off, 4)) {
    __imp__sub_8219E8F0(ctx, base);
    return;
  }
  uint32_t before_byte = 0;
  uint32_t before_word = 0;
  if (persist != 0) {
    before_byte = read8(base, persist + byte_off);
    before_word = read32(base, persist + word_off);
  }
  emit("WRITE_ENTER FUNC=0x8219E8F0 LR=0x%08X R3=0x%08X BIT=%u VALUE=%u BYTE=%u WORD=%u BASE=0x%08X BEFORE_BYTE=0x%02X BEFORE_WORD=0x%08X IN_WINDOW=%llu CALL=%llu",
       lr, r3, bit, value, byte_off, word_off, persist, before_byte, before_word,
       g_writer_in_window.load(), call_no);

  __imp__sub_8219E8F0(ctx, base);  // original guest body, exactly once

  uint32_t after_byte = 0;
  uint32_t after_word = 0;
  if (persist != 0) {
    after_byte = read8(base, persist + byte_off);
    after_word = read32(base, persist + word_off);
  }
  g_writer_in_window.fetch_add(1);
  emit("WRITE_AFTER FUNC=0x8219E8F0 BYTE=%u WORD=%u AFTER_BYTE=0x%02X AFTER_WORD=0x%08X CALL=%llu",
       byte_off, word_off, after_byte, after_word, call_no);
}

// ---- SECONDARY: inventory removal 0x8247A960 (removes IDs 914/915/916) ----
extern "C" void __imp__sub_8247A960(PPCContext& ctx, uint8_t* base);

extern "C" void sub_8247A960(PPCContext& ctx, uint8_t* base) {
  if (!trace_path()) {  // disabled: pure pass-through
    __imp__sub_8247A960(ctx, base);
    return;
  }
  const uint32_t lr = static_cast<uint32_t>(ctx.lr);
  const uint32_t r3 = ctx.r3.u32;
  const uint32_t r4 = ctx.r4.u32;
  const uint32_t r5 = ctx.r5.u32;
  // The original body derives its inventory container as r3 + 14912 (0x3A40).
  const uint32_t container = r3 + 14912;
  emit("INVENTORY_ENTER FUNC=0x8247A960 LR=0x%08X R3=0x%08X R4=0x%08X R5=0x%08X R3p14912=0x%08X",
       lr, r3, r4, r5, container);

  __imp__sub_8247A960(ctx, base);  // original guest body, exactly once

  emit("INVENTORY_EXIT FUNC=0x8247A960 LR=0x%08X RET=0x%08X", lr, ctx.r3.u32);
}

}  // namespace vesp
