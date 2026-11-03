#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>

namespace iu::guest_memory {
// Physical guest addresses include the SDK mapping bias.
inline uint8_t* host_ptr(uint8_t* base, uint32_t guest) {
  uintptr_t offset = (guest >= 0xE0000000u) ? (static_cast<uintptr_t>(guest) + 0x1000u)
                                            : static_cast<uintptr_t>(guest);
  return base + offset;
}

// True if the guest range is committed and readable in host memory.
inline bool readable(uint8_t* base, uint32_t guest, uint32_t len = 4) {
  if (!base || guest < 0x10000u || uint64_t(guest) + len > 0x100000000ull) return false;
  const uint8_t* p = host_ptr(base, guest);
  MEMORY_BASIC_INFORMATION mbi;
  if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
  if (mbi.State != MEM_COMMIT) return false;
  if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
  const uint8_t* end = static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
  return p + len <= end;
}

inline uint32_t read32(uint8_t* base, uint32_t guest) {
  return __builtin_bswap32(*(volatile uint32_t*)host_ptr(base, guest));
}
}  // namespace iu::guest_memory
