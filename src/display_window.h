#pragma once
#include <rex/ui/window.h>
#include <rex/cvar.h>
#include <rex/graphics/video_mode_util.h>
#include <SDL3/SDL.h>
#include <vector>
#include <functional>

namespace iu::ui {
inline SDL_Window* FindSDLWindow(rex::ui::Window* window) {
  if (!window) return nullptr;
  int count=0; auto** windows=SDL_GetWindows(&count);
  SDL_Window* result=nullptr;
  for(int i=0;i<count;++i)
    if(SDL_GetPointerProperty(SDL_GetWindowProperties(windows[i]),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr)==window->GetNativeWindowHandle()) result=windows[i];
  SDL_free(windows); return result;
}
struct MonitorChoice { int index; std::string name; };
inline std::vector<MonitorChoice> MonitorChoices() {
  std::vector<MonitorChoice> result{{0,"Default display"}};
  int count=0; auto* displays=SDL_GetDisplays(&count);
  for(int i=0;i<count;++i) {
    const char* name=SDL_GetDisplayName(displays[i]);
    result.push_back({i+1,std::to_string(i+1)+": "+(name?name:"Display")});
  }
  SDL_free(displays); return result;
}
class DisplayWindow {
 public:
  void Initialize(rex::ui::Window* window, std::function<void()> on_applied={}) { window_=window; on_applied_=std::move(on_applied); }
  const std::string& Error() const { return error_; }
  bool Apply(const std::function<void()>& change) {
    error_.clear();
    auto* window=FindSDLWindow(window_);
    if(!window) { error_="Could not find the game window."; return false; }
    const auto old_monitor=rex::cvar::GetFlagByName("monitor");
    const auto old_width=rex::cvar::GetFlagByName("window_width");
    const auto old_height=rex::cvar::GetFlagByName("window_height");
    change();
    // Fullscreen already has an SDK live callback; aspect ratio is read by
    // the presenter. Only rebuild window placement when size/monitor changed.
    if(old_monitor==rex::cvar::GetFlagByName("monitor") &&
       old_width==rex::cvar::GetFlagByName("window_width") &&
       old_height==rex::cvar::GetFlagByName("window_height")) {
      if(on_applied_) on_applied_();
      return true;
    }
    window_->SetFullscreen(false);
    bool success=true;
    const int monitor=rex::cvar::Query<int32_t>("monitor");
    int count=0; auto* displays=SDL_GetDisplays(&count);
    if (monitor>0 && displays && monitor<=count) {
      const auto pos=SDL_WINDOWPOS_CENTERED_DISPLAY(displays[monitor-1]);
      success=SDL_SetWindowPosition(window,pos,pos);
    } else if(monitor>0) success=false;
    else {
      const auto pos=SDL_WINDOWPOS_CENTERED_DISPLAY(SDL_GetPrimaryDisplay());
      success=SDL_SetWindowPosition(window,pos,pos);
    }
    SDL_free(displays);
    int w=rex::cvar::Query<int32_t>("window_width"),h=rex::cvar::Query<int32_t>("window_height");
    int32_t preset_w=1280,preset_h=720;
    rex::graphics::video_mode_util::TryGetResolutionPresetFromCVar(preset_w,preset_h);
    if(w==0) w=rex::cvar::HasNonDefaultValue("video_mode_width")?rex::cvar::Query<int32_t>("video_mode_width"):preset_w;
    if(h==0) h=rex::cvar::HasNonDefaultValue("video_mode_height")?rex::cvar::Query<int32_t>("video_mode_height"):preset_h;
    success=SDL_SetWindowSize(window,w,h)&&success;
    window_->SetFullscreen(rex::cvar::Query<bool>("fullscreen"));
    if(success) { if(on_applied_) on_applied_(); }
    else error_="Display change failed. Save and restart to retry: "+std::string(SDL_GetError());
    return success;
  }
 private:
  rex::ui::Window* window_=nullptr;
  std::function<void()> on_applied_;
  std::string error_;
};
} // namespace iu::ui
