#pragma once
#include "menu_pause.h"
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace iu::saves {
// Track the whole mounted save session, including reads: OPEN_EXISTING may
// subsequently be written. Pending opens and post-close backup I/O also count.
// One mutex arbitrates save starts against session exits, closing the race
// between checking a disabled button and actually terminating the process.
class Activity {
 public:
  using Id = uint64_t;
  enum class Result { Pending, Open, Failed, RootClosed };
  using Completion = std::function<Result()>;
  explicit Activity(std::function<void(bool)> changed = {}) : changed_(std::move(changed)) {}

  Id Begin(std::string root = {}) {
    std::unique_lock lock(mutex_);
    // Once an exit is committed, no new save may enter the SDK. A failed
    // relaunch cancels this reservation and wakes the caller unchanged.
    wake_.wait(lock, [this] { return !exiting_; });
    const auto id = ++next_;
    entries_.emplace(id, Entry{Normalize(std::move(root)), {}});
    Publish();
    return id;
  }
  void End(Id id) {
    std::lock_guard lock(mutex_);
    entries_.erase(id); Publish();
  }
  void Pending(Id id, Completion completion) {
    std::lock_guard lock(mutex_);
    if (auto it = entries_.find(id); it != entries_.end()) it->second.completion = std::move(completion);
  }
  void Refresh() {
    std::lock_guard lock(mutex_);
    for (auto it = entries_.begin(); it != entries_.end();) {
      auto& completion = it->second.completion;
      if (completion) {
        auto result = completion();
        if (result != Result::Pending) {
          if (result == Result::RootClosed) {
            // A failed CREATE_ALWAYS may already have unmounted an older
            // session. Clear it too, without touching a newer root generation.
            const auto id = it->first;
            const auto root = it->second.root;
            for (auto old = entries_.begin(); old != entries_.end() && old->first <= id;) {
              if (old->second.root == root) old = entries_.erase(old); else ++old;
            }
            it = entries_.upper_bound(id); continue;
          }
          if (result == Result::Failed) { it = entries_.erase(it); continue; }
          completion = {}; // Stop reading guest overlapped memory once consumed.
        }
      }
      ++it;
    }
    Publish();
  }
  std::vector<Id> ForRoot(std::string root) {
    std::lock_guard lock(mutex_);
    root = Normalize(std::move(root));
    std::vector<Id> ids;
    for (const auto& [id, entry] : entries_) if (entry.root == root) ids.push_back(id);
    return ids;
  }
  void Closed(const std::vector<Id>& ids) {
    std::lock_guard lock(mutex_);
    for (auto id : ids) entries_.erase(id);
    Publish();
  }
  bool Busy() const { std::lock_guard lock(mutex_); return !entries_.empty(); }
  bool TryReserveExit() {
    Refresh();
    std::lock_guard lock(mutex_);
    if (exiting_ || !entries_.empty()) return false;
    exiting_ = true; return true;
  }
  bool ExitReserved() const { std::lock_guard lock(mutex_); return exiting_; }
  void CancelExit() {
    { std::lock_guard lock(mutex_); exiting_ = false; }
    wake_.notify_all();
  }

 private:
  struct Entry { std::string root; Completion completion; };
  static std::string Normalize(std::string root) {
    for (auto& c : root) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return root;
  }
  void Publish() {
    const bool busy = !entries_.empty();
    if (busy != published_busy_) { published_busy_ = busy; if (changed_) changed_(busy); }
  }
  mutable std::mutex mutex_;
  std::condition_variable wake_;
  std::map<Id, Entry> entries_;
  Id next_ = 0;
  bool exiting_ = false;
  bool published_busy_ = false;
  std::function<void(bool)> changed_;
};

inline Activity activity([](bool busy) { iu::player::menu_pause.SetSaveBusy(busy); });
inline constexpr const char* kBusyMessage = "Save data is in use. Restart, restore and exit are unavailable until it finishes.";
} // namespace iu::saves
