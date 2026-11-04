#include "../src/save_activity.h"
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

using iu::saves::Activity;
using Result = Activity::Result;
using namespace std::chrono_literals;
static void Check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
template<class Predicate> void Until(Predicate ready) {
  const auto deadline=std::chrono::steady_clock::now()+2s;
  while(!ready()) {Check(std::chrono::steady_clock::now()<deadline,"Timed out");std::this_thread::yield();}
}
int main() {
  try {
    Activity saves;
    Check(saves.TryReserveExit(),"Idle exit allowed");saves.CancelExit();
    auto open=saves.Begin("SAVE");
    Check(!saves.TryReserveExit(),"Block exit before create call begins");
    std::atomic<Result> result{Result::Pending};
    saves.Pending(open,[&] {return result.load();});
    saves.Refresh();
    Check(!saves.TryReserveExit(),"Queued mount blocks exit");
    result=Result::Open;saves.Refresh();
    result=Result::Failed;saves.Refresh(); // guest reuses its overlapped memory
    Check(saves.Busy(),"Successful mount no longer reads reused overlapped memory");
    auto same_root=saves.ForRoot("sAvE");
    Check(same_root.size()==1,"Root matching is case insensitive");
    auto backup=saves.Begin();
    // A failed close must not call Closed. Closing a container successfully
    // still cannot unlock restart until the post-close backup has finished.
    Check(!saves.TryReserveExit(),"Failed close remains protected");
    saves.Closed(same_root);
    Check(!saves.TryReserveExit(),"Post-close backup still blocks exit");
    saves.End(backup);
    Check(saves.TryReserveExit(),"Close and backup completion allow exit");saves.CancelExit();

    auto a=saves.Begin("slot-a"), b=saves.Begin("slot-b");
    saves.End(a);Check(saves.Busy(),"Another slot remains protected");
    saves.Pending(b,[]{return Result::Failed;});saves.Refresh();
    Check(!saves.Busy(),"Failed asynchronous open releases protection");
    auto old=saves.Begin("save");
    auto failed=saves.Begin("save");
    saves.Pending(failed,[]{return Result::Failed;});saves.Refresh();
    Check(saves.ForRoot("save")==std::vector<Activity::Id>{old},"Duplicate mount failure preserves original mount");
    auto replaced=saves.Begin("save");
    saves.Pending(replaced,[]{return Result::RootClosed;});
    auto newer=saves.Begin("save");
    saves.Refresh();
    Check(saves.ForRoot("save")==std::vector<Activity::Id>{newer},"Failed overwrite clears old mount but not newer generation");
    auto stale=saves.ForRoot("save");
    auto latest=saves.Begin("save");saves.Closed(stale);
    Check(saves.ForRoot("save")==std::vector<Activity::Id>{latest},"Stale close cannot clear a newer session");
    saves.End(latest);

    // Exit wins the race: the next write cannot start. If process creation
    // fails, cancelling the reservation lets the exact original call proceed.
    Check(saves.TryReserveExit(),"Reserve idle exit");
    std::promise<void> attempted, entered;
    auto attempt=attempted.get_future(),entry=entered.get_future();
    std::thread writer([&] {attempted.set_value();auto id=saves.Begin();entered.set_value();saves.End(id);});
    struct Join { Activity& saves; std::thread& thread; ~Join(){saves.CancelExit();thread.join();} } join{saves,writer};
    attempt.wait();
    Check(entry.wait_for(30ms)==std::future_status::timeout,"No write enters after exit reservation");
    saves.CancelExit();
    Check(entry.wait_for(2s)==std::future_status::ready,"Failed restart releases pending writes");

    // Pause integration: a save spanning frames must finish even with F4 open.
    iu::player::MenuPause pause;
    Activity active([&](bool busy){pause.SetSaveBusy(busy);});
    auto save=active.Begin("save");pause.Request(true);
    pause.FrameBoundary();Check(!pause.IsPaused(),"F4 does not park an active save");
    active.End(save);
    std::atomic<bool> returned{false};
    std::thread frame([&] {pause.FrameBoundary();returned=true;});
    struct Resume { iu::player::MenuPause& pause;std::thread& frame;~Resume(){pause.Shutdown();frame.join();} } resume{pause,frame};
    Until([&]{return pause.IsPaused();});
    auto background=active.Begin(); // save starts after frame was already parked
    Until([&]{return returned.load();});
    Check(!pause.IsPaused(),"Background save wakes parked frame");
    active.End(background);
    Check(!active.Busy(),"Save completion clears activity");
    std::cout<<"Save activity and exit protection checks passed\n";
    return 0;
  } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
