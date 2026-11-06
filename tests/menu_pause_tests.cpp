// Exercise the real hook wrappers with a fake guest frame/timer. No assets or
// saves are needed, and a failed check always releases the waiting thread.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>

struct PPCContext { uint32_t result = 0; };
namespace iu::save_anywhere { void PollAndDispatch(PPCContext&, uint8_t*) {} }
#include "../src/menu_pause_hook.h"

using namespace std::chrono_literals;
static std::atomic<unsigned> frames{0}, ticks{0};
extern "C" void __imp__sub_821B7948(PPCContext& ctx, uint8_t*) {
  ctx.result = 123;
  ++frames;
}
extern "C" void __imp__sub_821FC010(PPCContext&, uint8_t*) { ++ticks; }
static void Check(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
template<class Predicate> void Until(Predicate ready) {
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (!ready()) {
    Check(std::chrono::steady_clock::now() < deadline, "Pause/resume timed out");
    std::this_thread::yield();
  }
}
struct Frame {
  std::atomic<bool> returned{false};
  PPCContext ctx;
  std::thread worker{[this] { sub_821B7948(ctx, nullptr); returned = true; }};
  ~Frame() {
    iu::player::menu_pause.Request(false);
    worker.join();
  }
};
int main() {
  try {
    auto& pause = iu::player::menu_pause;
    PPCContext ctx;
    sub_821B7948(ctx, nullptr);
    Check(frames == 1 && ctx.result == 123, "Normal frame result preserved");

    // Requests must not stop vblanks before the frame finishes its own wait.
    pause.Request(true);
    sub_821FC010(ctx, nullptr);
    Check(ticks == 1 && !pause.IsPaused(), "Pending pause allows frame timer");
    {
      Frame frame;
      Until([&] { return pause.IsPaused(); });
      Check(frames == 2 && !frame.returned, "Frame work completes before parking");
      for (unsigned i = 0; i < 600; ++i) sub_821FC010(ctx, nullptr);
      Check(ticks == 1 && !frame.returned, "Menu time does not advance simulation ticks");
      pause.Request(true); // Switching tabs/repeated opens must not unpause.
      Check(pause.IsPaused(), "Repeated open stays paused");
      pause.Request(false);
      Until([&] { return frame.returned.load(); });
      Check(!pause.IsPaused() && frame.ctx.result == 123, "Close resumes with guest result intact");
    }
    sub_821FC010(ctx, nullptr);
    Check(ticks == 2, "Resume advances one tick without catching up menu time");

    pause.Request(true);
    pause.Request(false);
    sub_821B7948(ctx, nullptr);
    Check(!pause.IsPaused(), "Closing before boundary cancels pause");
    for (unsigned i = 0; i < 50; ++i) {
      pause.Request(true);
      Frame frame;
      Until([&] { return pause.IsPaused(); });
      pause.Request(false);
      Until([&] { return frame.returned.load(); });
    }

    pause.Request(true);
    {
      Frame frame;
      Until([&] { return pause.IsPaused(); });
      pause.Shutdown();
      Until([&] { return frame.returned.load(); });
      Check(!pause.IsPaused(), "Shutdown releases parked guest");
    }
    pause.Request(true);
    sub_821B7948(ctx, nullptr);
    Check(!pause.IsPaused(), "Shutdown prevents subsequent parking");
    std::cout << "Menu pause checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
