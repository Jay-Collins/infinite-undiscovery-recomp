#pragma once
#include "controller_driver.h"
#include "frame_counter.h"
#include <chrono>
#include <rex/ui/imgui_dialog.h>
#include <imgui.h>

namespace iu::ui {
class PlayerOverlay final : public rex::ui::ImGuiDialog {
 public:
  PlayerOverlay(rex::ui::ImGuiDrawer* drawer, rex::ui::Window* window,
                iu::player::ControllerDriver* controller,
                std::function<bool()> menu_open, std::function<void()> toggle)
      : ImGuiDialog(drawer), window_(window), controller_(controller),
        menu_open_(std::move(menu_open)), toggle_(std::move(toggle)) {}
 private:
  void OnDraw(ImGuiIO& io) override {
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    const auto raw = controller_ && window_->HasFocus() ? controller_->PollRaw() : rex::input::X_INPUT_GAMEPAD{};
    const uint16_t buttons=raw.buttons;
    const bool chord=(buttons&0x30)==0x30;
    if(chord && !chord_down_) toggle_();
    chord_down_=chord;
    const bool menu=menu_open_();
    const std::pair<ImGuiKey,uint16_t> keys[] = {
      {ImGuiKey_GamepadFaceDown,0x1000},{ImGuiKey_GamepadFaceRight,0x2000},
      {ImGuiKey_GamepadFaceLeft,0x4000},{ImGuiKey_GamepadFaceUp,0x8000},
      {ImGuiKey_GamepadDpadUp,1},{ImGuiKey_GamepadDpadDown,2},
      {ImGuiKey_GamepadDpadLeft,4},{ImGuiKey_GamepadDpadRight,8},
      {ImGuiKey_GamepadL1,0x100},{ImGuiKey_GamepadR1,0x200}};
    for(auto [key,mask]:keys) io.AddKeyEvent(key,menu && !chord && (buttons&mask));
    const auto axis=[&](ImGuiKey key,float value) { const float strength=menu?std::clamp((value-0.25f)/0.75f,0.0f,1.0f):0; io.AddKeyAnalogEvent(key,strength>0,strength); };
    axis(ImGuiKey_GamepadLStickLeft,-float(int16_t(raw.thumb_lx))/32768);
    axis(ImGuiKey_GamepadLStickRight,float(int16_t(raw.thumb_lx))/32768);
    axis(ImGuiKey_GamepadLStickUp,float(int16_t(raw.thumb_ly))/32768);
    axis(ImGuiKey_GamepadLStickDown,-float(int16_t(raw.thumb_ly))/32768);

    const auto now=std::chrono::steady_clock::now();
    const auto count=iu::player::submitted_frames.load(std::memory_order_relaxed);
    const double elapsed=std::chrono::duration<double>(now-last_sample_).count();
    if(elapsed>=0.5) { fps_=(count-last_count_)/elapsed; last_count_=count; last_sample_=now; }
    const int performance=rex::cvar::Query<int32_t>("iu_performance");
    if(performance) {
      ImGui::SetNextWindowPos(ImVec2(12,12)); ImGui::SetNextWindowBgAlpha(0.75f);
      if(ImGui::Begin("Performance##player",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::Text("Game: %.1f FPS",fps_);
        if(performance==2) {
          if(fps_>0) ImGui::Text("Average frame: %.2f ms",1000.0/fps_);
          else ImGui::TextUnformatted("Waiting for game frames");
        }
      }
      ImGui::End();
    }
  }
  rex::ui::Window* window_;
  iu::player::ControllerDriver* controller_;
  std::function<bool()> menu_open_;
  std::function<void()> toggle_;
  bool chord_down_=false;
  double fps_=0;
  uint64_t last_count_=0;
  std::chrono::steady_clock::time_point last_sample_=std::chrono::steady_clock::now();
};
} // namespace iu::ui
