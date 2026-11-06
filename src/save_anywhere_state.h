#pragma once
#include <cstdint>
#include <mutex>
#include <optional>

namespace iu::save_anywhere {
enum class Availability { Unknown, Ready, Event, Combat, Menu, Saving, Active, Changed, Failed };
inline const char* Explain(Availability state) {
  switch (state) {
    case Availability::Ready: return "Open the game's save-slot screen.";
    case Availability::Event: return "Unavailable during cutscenes, scripted events or transitions.";
    case Availability::Combat: return "Unavailable while the party is in combat or has weapons drawn.";
    case Availability::Menu: return "Close the game's other menus or dialogue first.";
    case Availability::Saving: return "Wait for the current save operation to finish.";
    case Availability::Active: return "The save screen is already open or opening.";
    case Availability::Changed: return "The game state changed. Close and reopen F4 to try again.";
    case Availability::Failed: return "The save screen could not be opened.";
    default: return "Available after loading a game and returning to normal gameplay.";
  }
}

struct Snapshot {
  Availability availability = Availability::Unknown;
  uint32_t player = 0, party = 0, world = 0;
  bool SameSession(const Snapshot& other) const {
    return player == other.player && party == other.party && world == other.world;
  }
};

// Reader exposes checked big-endian U32/U8 reads. No guest calls or writes:
// this is shared with fixture tests of the actual title-memory predicates.
template<class Reader> Snapshot Inspect(Reader& m) {
  constexpr uint32_t global = 0x82A58EA8, registry = 0x82A380F8;
  Snapshot out;
  uint32_t flags, table, ui, service, value;
  auto word = [&](uint32_t address, uint32_t& result) { return m.U32(address, result); };
  if (!word(global+112, flags) || !word(registry+24, table) || !table) return out;
  // Bit 1 is the native scripted-event mode (824EC2A0/824ED090).
  // Also reject loading, camp, scripted player control and transition locks.
  constexpr uint32_t locks = (1u<<0)|(1u<<1)|(1u<<2)|(1u<<3)|(1u<<5)|
      (1u<<6)|(1u<<11)|(1u<<12)|(1u<<15)|(1u<<24)|(1u<<29)|(1u<<30)|(1u<<31);
  if (flags & locks) { out.availability=Availability::Event; return out; }
  if (!word(registry+48, ui) || !ui || !word(ui+212, value)) return out;
  if (value) { out.availability=Availability::Menu; return out; }
  if (!word(global+8, service) || !service || !m.U8(service+64, value) || value!=1) return out;
  if (!m.U8(service+72, value) || value!=1) { out.availability=Availability::Menu; return out; }
  // Native UI blocker checked by the save window itself (82485188).
  uint32_t blocker;
  if (!word(0x82A95998, blocker) || !blocker || !word(blocker+60, value)) return out;
  if (value) { out.availability=Availability::Menu; return out; }
  auto resolve = [&](uint32_t handle, unsigned type, uint32_t& object) {
    // sub_82147140: lower 16 bits index, upper 16 bits generation, type mask.
    uint32_t actual, types;
    const uint64_t slot=uint64_t(table)+180+uint64_t(handle & 0xffff)*4;
    return handle && slot<=UINT32_MAX && word(uint32_t(slot),object) && object &&
        word(object+16,actual) && actual==handle && word(object+28,types) && (types&(1u<<type));
  };
  uint32_t player_handle, party_handle;
  if (!word(global+120, player_handle) || !word(global+124, party_handle) ||
      !resolve(player_handle,19,out.player) || !resolve(party_handle,21,out.party) ||
      !resolve(201,20,out.world)) return out;
  uint32_t brain, enemies, incoming, first, last;
  if (!word(out.party+76,brain) || !brain || !word(brain+132,enemies) ||
      !word(brain+144,incoming) || !word(out.party+92,first) || !word(out.party+96,last)) return out;
  // CParty's target collections, consumed by the party combat AI. Requiring
  // both empty also covers an engaged party while the controlled actor sheathes.
  if (enemies || incoming) { out.availability=Availability::Combat; return out; }
  if (!first || last<=first || (last-first)%4 || last-first>64*4) return out;
  bool found_player=false;
  for (uint32_t entry=first; entry<last; entry+=4) {
    uint32_t handle, actor, stats, hp, battle;
    if (!word(entry,handle) || !resolve(handle,19,actor) ||
        !word(actor+3260,stats) || !stats || !word(stats+48,hp) || !word(stats+8,battle)) return out;
    found_player |= actor==out.player;
    if (actor==out.player && int32_t(hp)<=0) return out;
    // sub_8215DD08 reads this bit; 824B7D08 sets it alongside battle message 1101.
    if (int32_t(hp)>0 && (battle & (1u<<14))) { out.availability=Availability::Combat; return out; }
  }
  if (found_player) out.availability=Availability::Ready;
  return out;
}

// UI requests never touch guest memory. The frame thread consumes a request
// once and checks the fresh snapshot; rejected requests never fire later.
class Requests {
 public:
  Snapshot Get() const { std::lock_guard lock(mutex_); return snapshot_; }
  void Publish(Snapshot snapshot) { std::lock_guard lock(mutex_); snapshot_=snapshot; }
  bool Request() {
    std::lock_guard lock(mutex_);
    if (pending_ || snapshot_.availability!=Availability::Ready) return false;
    pending_=snapshot_;
    snapshot_.availability=Availability::Active;
    return true;
  }
  std::optional<Snapshot> Take() {
    std::lock_guard lock(mutex_);
    auto result=pending_; pending_.reset(); return result;
  }
 private:
  mutable std::mutex mutex_;
  Snapshot snapshot_;
  std::optional<Snapshot> pending_;
};
inline Requests requests;
} // namespace iu::save_anywhere
