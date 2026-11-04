#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace iu::player {
// Only the game frame thread waits here. UI, rendering, audio and file workers
// remain available; in particular, opening the menu during a save does not
// suspend a thread in the middle of a filesystem operation.
class MenuPause {
 public:
  void Request(bool pause) {
    {
      std::lock_guard lock(mutex_);
      requested_ = pause && !shutdown_;
    }
    wake_.notify_all();
  }

  void FrameBoundary() {
    std::unique_lock lock(mutex_);
    if (!requested_ || shutdown_) return;
    paused_.store(true, std::memory_order_release);
    wake_.wait(lock, [this] { return !requested_ || shutdown_; });
    paused_.store(false, std::memory_order_release);
  }

  bool IsPaused() const { return paused_.load(std::memory_order_acquire); }

  // Release the guest before title termination. Later frame calls cannot park.
  void Shutdown() {
    {
      std::lock_guard lock(mutex_);
      shutdown_ = true;
      requested_ = false;
    }
    wake_.notify_all();
  }

 private:
  std::mutex mutex_;
  std::condition_variable wake_;
  bool requested_ = false;
  bool shutdown_ = false;
  std::atomic<bool> paused_{false};
};

inline MenuPause menu_pause;
} // namespace iu::player
