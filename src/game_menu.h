#pragma once
#include "menu_pause.h"

#include "menu_settings.h"
#include "display_settings.h"
#include "display_window.h"
#include "player_options.h"
#include "save_backups.h"
#include "save_activity.h"
#include "save_anywhere_state.h"
#include "version.h"
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/overlay/achievement_icon_cache.h>
#include <rex/system/achievement_manager.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <functional>

namespace iu::ui {

enum class MenuTab { Settings, Controls, Achievements, Game };

class GameMenu final : public rex::ui::ImGuiDialog {
 public:
  GameMenu(rex::ui::ImGuiDrawer* drawer, rex::ui::ImmediateDrawer* immediate,
           rex::Runtime* runtime, rex::system::AchievementManager* achievements,
           MenuSettings& settings, std::filesystem::path config, std::string profile,
           std::function<void()> close, std::function<void()> profiles,
           std::function<void()> quit, std::shared_ptr<iu::saves::Backups> backups,
           std::function<bool(const std::string&,std::string&)> restart, DisplayWindow& display,
           std::function<void(std::function<void()>)> apply_display_change)
      : ImGuiDialog(drawer), achievements_(achievements), icons_(immediate, runtime),
        settings_(settings), config_(std::move(config)), profile_(std::move(profile)),
        close_(std::move(close)), profiles_(std::move(profiles)), quit_(std::move(quit)),
        display_(display), apply_display_change_(std::move(apply_display_change)),
        backups_(std::move(backups)), restart_(std::move(restart)) {
    if(backups_)backup_list_=backups_->List();
  }

  void Select(MenuTab tab) { requested_tab_ = static_cast<int>(tab); }

 private:
  bool SaveSettings() {
    save_failed_=!settings_.Save(config_,save_error_);
    message_=save_failed_?"Could not save settings. Hover for details.":"Settings saved for this profile.";
    return !save_failed_;
  }
  void DrawBackups() {
    ImGui::SeparatorText("Save backups");
    ImGui::TextWrapped("Automatic backups keep the latest 10 versions per slot. Existing saves are backed up at launch; changed saves are copied when the game closes their save container.");
    if(!backups_){ImGui::TextDisabled("Backups are unavailable.");return;}
    ImGui::TextWrapped("%s",backups_->Status().c_str());
    if(ImGui::Button("Refresh backups"))backup_list_=backups_->List();
    ImGui::TextDisabled("Times are local. Restore replaces one slot and restarts the game.");
    if(backup_list_.empty())ImGui::TextDisabled("No backups yet. Save normally in the game to create one.");
    for(const auto& backup:backup_list_) {
      ImGui::PushID(backup.id.c_str());
      ImGui::Text("%s  |  %s",backup.created.c_str(),backup.slot.c_str());
      ImGui::TextDisabled("Account %s  |  %.0f KB",backup.account.c_str(),double(backup.bytes)/1024);
      if(!backup.can_restore)ImGui::TextDisabled("Incomplete recovery copy; kept for inspection, cannot restore.");
      ImGui::BeginDisabled(!backup.can_restore || iu::saves::activity.Busy());
      if(ImGui::Button("Restore this backup...")) {
        restore_selection_=backup;request_restore_confirmation_=true;
      }
      ImGui::EndDisabled();
      ImGui::PopID();
    }
  }
  static constexpr ImVec4 kAccent{0.48f, 0.78f, 0.96f, 1.0f};

  static bool ValidBinding(std::string_view value) {
    if (value.empty()) return true;  // Unassigned.
    while (!value.empty()) {
      const auto comma = value.find(',');
      auto key = value.substr(0, comma);
      while (!key.empty() && key.front() == ' ') key.remove_prefix(1);
      while (!key.empty() && key.back() == ' ') key.remove_suffix(1);
      while (key.find('+') != std::string_view::npos) {
        const auto plus = key.find('+');
        const auto modifier = key.substr(0, plus);
        if (modifier != "Shift" && modifier != "Ctrl" && modifier != "Control" && modifier != "Alt")
          return false;
        key.remove_prefix(plus + 1);
      }
      const auto vk = rex::ui::ParseVirtualKey(key);
      if (vk == rex::ui::VirtualKey::kNone || vk == rex::ui::VirtualKey::kShift ||
          vk == rex::ui::VirtualKey::kControl || vk == rex::ui::VirtualKey::kMenu)
        return false;
      // These keys belong to the host and cannot also control gameplay.
      if (vk == rex::ui::VirtualKey::kF4 || vk == rex::ui::VirtualKey::kF7 ||
          vk == rex::ui::VirtualKey::kF10 || vk == rex::ui::VirtualKey::kF12)
        return false;
      if (comma == std::string_view::npos) return true;
      value.remove_prefix(comma + 1);
      if (value.empty()) return false;
    }
    return true;
  }

  void Set(const char* name, const std::string& value) {
    if(std::string_view(name)=="fullscreen" || std::string_view(name)=="present_letterbox") {
      const std::string owned_name=name;
      apply_display_change_([owned_name,value] { rex::cvar::SetFlagByName(owned_name,value); });
      message_.clear(); return;
    }
    if (rex::cvar::SetFlagByName(name, value)) {
      message_.clear();
      save_failed_ = false;
    } else {
      message_ = "This value could not be applied.";
      save_failed_ = true;
    }
  }

  void Setting(const MenuSetting& setting) {
    const auto* flag = rex::cvar::GetFlagInfo(setting.name);
    if (!flag) return;
    ImGui::PushID(setting.name);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(setting.label);
    if (*setting.help && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", setting.help);
    if (flag->lifecycle == rex::cvar::Lifecycle::kRequiresRestart) {
      ImGui::SameLine();
      ImGui::TextDisabled("*");
    }
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1);
    const std::string current = flag->getter();
    const bool disabled = flag->lifecycle == rex::cvar::Lifecycle::kInitOnly;
    ImGui::BeginDisabled(disabled);
    if (flag->type == rex::cvar::FlagType::Boolean) {
      bool value = current == "true";
      if (ImGui::Checkbox("##value", &value)) Set(setting.name, value ? "true" : "false");
    } else if (flag->type == rex::cvar::FlagType::Int32) {
      int value = std::stoi(current);
      bool changed=false;
      if(std::string_view(setting.name)=="iu_ui_scale") {
        const std::string label=std::to_string(value)+"%";
        if(ImGui::BeginCombo("##value",label.c_str())) {
          for(int scale:{75,100,125,150,175,200}) {
            const std::string option=std::to_string(scale)+"%";
            if(ImGui::Selectable(option.c_str(),value==scale)) { value=scale; changed=true; }
          }
          ImGui::EndCombo();
        }
      } else if(std::string_view(setting.name).starts_with("iu_")) {
        changed=ImGui::SliderInt("##value",&value,int(flag->constraints.min.value_or(0)),int(flag->constraints.max.value_or(100)),"%d%%");
      } else changed=ImGui::InputInt("##value", &value);
      if (changed) {
        if (flag->constraints.min) value = std::max(value, static_cast<int>(*flag->constraints.min));
        if (flag->constraints.max) value = std::min(value, static_cast<int>(*flag->constraints.max));
        Set(setting.name, std::to_string(value));
      }
    } else if (flag->type == rex::cvar::FlagType::Double) {
      float value = static_cast<float>(std::stod(current));
      if (ImGui::SliderFloat("##value", &value, 0.01f, 10.0f, "%.2f"))
        Set(setting.name, std::to_string(value));
    } else if (!flag->constraints.allowed_values.empty()) {
      auto pretty = [](const std::string& s) -> const char* {
        if (s == "none") return "Off";
        if (s == "fxaa") return "FXAA";
        if (s == "fxaa_extreme") return "FXAA (strong)";
        if (s == "30") return "30 FPS (original)";
        if (s == "60") return "60 FPS (experimental)";
        return s.c_str();
      };
      if (ImGui::BeginCombo("##value", pretty(current))) {
        for (const auto& option : flag->constraints.allowed_values) {
          if (ImGui::Selectable(pretty(option), current == option)) Set(setting.name, option);
        }
        ImGui::EndCombo();
      }
    } else if (flag->type == rex::cvar::FlagType::String) {
      auto [it, inserted] = binding_buffers_.try_emplace(setting.name);
      auto& buffer = it->second;
      if (inserted) snprintf(buffer.data(), buffer.size(), "%s", current.c_str());
      ImGui::InputText("##value", buffer.data(), buffer.size());
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (ValidBinding(buffer.data())) Set(setting.name, buffer.data());
        else {
          message_ = "Invalid binding. Use names such as Space, W or Shift+Up. F4/F7/F10/F12 are reserved.";
          save_failed_ = true;
          snprintf(buffer.data(), buffer.size(), "%s", current.c_str());
        }
      }
    }
    ImGui::EndDisabled();
    ImGui::PopID();
  }

  static void DisplayRow(const char* label, const char* help) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", help);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1);
  }

  void DisplayResult(bool success) {
    save_failed_ = !success;
    message_ = success ? "" : "This display setting could not be applied.";
  }

  void DisplayOptions() {
    DisplayRow("Monitor", "Choose the display used for the game window. Changes apply immediately.");
    const auto monitors=MonitorChoices();
    const int current_monitor=rex::cvar::Query<int32_t>("monitor");
    std::string monitor_name="Disconnected display";
    for(const auto& choice:monitors) if(choice.index==current_monitor) monitor_name=choice.name;
    if(ImGui::BeginCombo("##monitor",monitor_name.c_str())) {
      for(const auto& choice:monitors) if(ImGui::Selectable(choice.name.c_str(),choice.index==current_monitor)) {
        apply_display_change_([index=choice.index] { rex::cvar::SetFlagByName("monitor",std::to_string(index)); });
      }
      ImGui::EndCombo();
    }
    DisplayRow("Aspect ratio", "Original preserves the game's proportions with bars where needed. Stretch fills the display; it does not expand the camera's field of view.");
    const bool original = rex::cvar::GetFlagByName("present_letterbox") == "true";
    if (ImGui::BeginCombo("##aspect", original ? "Original (16:9)" : "Match display (stretch)")) {
      if (ImGui::Selectable("Original (16:9)", original)) Set("present_letterbox", "true");
      if (ImGui::Selectable("Match display (stretch)", !original)) Set("present_letterbox", "false");
      ImGui::EndCombo();
    }

    const bool fullscreen = rex::cvar::GetFlagByName("fullscreen") == "true";
    DisplayRow("Display resolution",
               "Borderless fullscreen follows your desktop. Windowed presets apply immediately. Rendering quality is controlled separately by Internal resolution.");
    const int width = std::stoi(rex::cvar::GetFlagByName("window_width"));
    const int height = std::stoi(rex::cvar::GetFlagByName("window_height"));
    std::string preview = std::to_string(width) + " x " + std::to_string(height) + " (custom)";
    for (const auto& preset : kDisplayResolutions)
      if (width == preset.width && height == preset.height) preview = preset.label;
    if (fullscreen) preview = "Desktop (borderless)";
    ImGui::BeginDisabled(fullscreen);
    if (ImGui::BeginCombo("##display_resolution", preview.c_str())) {
      for (const auto& preset : kDisplayResolutions) {
        if (ImGui::Selectable(preset.label, width == preset.width && height == preset.height))
          apply_display_change_([width=preset.width,height=preset.height] { SetDisplayResolution(width,height); });
      }
      ImGui::EndCombo();
    }
    ImGui::EndDisabled();
  }

  void InternalResolution() {
    DisplayRow("Internal resolution *", "Render at a uniform multiple of the game's native resolution. Higher scales cost GPU performance and memory. Save and restart to apply.");
    const auto [x, y] = EffectiveInternalScale();
    std::string preview = x == y ? (x == 1 ? "Native (1x)" : std::to_string(x) + "x native")
                                : "Custom (" + std::to_string(x) + "x / " + std::to_string(y) + "x)";
    const bool available = rex::cvar::GetFlagInfo("resolution_scale") &&
                           rex::cvar::GetFlagInfo("draw_resolution_scale_x") &&
                           rex::cvar::GetFlagInfo("draw_resolution_scale_y");
    ImGui::BeginDisabled(!available);
    if (ImGui::BeginCombo("##internal_resolution", available ? preview.c_str() : "Unavailable")) {
      for (int scale = 1; scale <= 8; ++scale) {
        const auto label = scale == 1 ? std::string("Native (1x)") : std::to_string(scale) + "x native";
        if (ImGui::Selectable(label.c_str(), x == scale && y == scale)) {
          const bool success=SetInternalScale(scale);
          DisplayResult(success);
          if(success) message_="Internal resolution changed. Save settings, then restart the game.";
        }
      }
      ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("Save settings and restart the game to apply. This does not change rendering while playing.");
  }

  void Group(const char* name, int group) {
    ImGui::Spacing();
    ImGui::SeparatorText(name);
    if (ImGui::BeginTable(name, 2, ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch, 1.2f);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
      for (const auto& setting : kMenuSettings) if (setting.group == group) Setting(setting);
      if (group == 0) DisplayOptions();
      if (group == 1) InternalResolution();
      if (group == 1) {
        DisplayRow("Performance display", "Game frame submissions measured over a half-second window. Average frame time is calculated from those submissions.");
        const char* modes[]={"Off","FPS","FPS + frame time"};
        const int mode=std::clamp(rex::cvar::Query<int32_t>("iu_performance"),0,2);
        if(ImGui::BeginCombo("##performance",modes[mode])) {
          for(int i=0;i<3;++i) if(ImGui::Selectable(modes[i],mode==i)) Set("iu_performance",std::to_string(i));
          ImGui::EndCombo();
        }
      }
      ImGui::EndTable();
    }
  }

  void ResetTab(bool controls) {
    for (const auto& setting : kMenuSettings) {
      if (setting.group == 7) continue; // Game options do not belong to either resettable tab.
      if ((setting.group >= 3) != controls) continue;
      if (const auto* flag = rex::cvar::GetFlagInfo(setting.name)) {
        // Apply window settings together after resetting other settings.
        if(setting.group==-1 && (std::string_view(setting.name)=="monitor" || std::string_view(setting.name)=="window_width" || std::string_view(setting.name)=="window_height" || std::string_view(setting.name)=="present_letterbox")) continue;
        if(std::string_view(setting.name)=="fullscreen") continue;
        Set(setting.name, flag->default_value);
      }
    }
    if(!controls) apply_display_change_([] {
      for(const char* name:{"fullscreen","monitor","window_width","window_height","present_letterbox"})
        if(const auto* flag=rex::cvar::GetFlagInfo(name)) rex::cvar::SetFlagByName(name,flag->default_value);
    });
    binding_buffers_.clear();
  }

  void DrawAchievements() {
    if (!achievements_) { ImGui::TextDisabled("Achievements are not available yet."); return; }
    const auto list = achievements_->ListAchievements();
    int unlocked = 0, earned = 0, total = 0;
    for (const auto& achievement : list) {
      total += achievement.gamerscore;
      if (achievements_->IsUnlocked(achievement.id)) { ++unlocked; earned += achievement.gamerscore; }
    }
    ImGui::TextColored(kAccent, "%d / %d unlocked     %d / %d G", unlocked, static_cast<int>(list.size()), earned, total);
    ImGui::ProgressBar(list.empty() ? 0.0f : float(unlocked) / list.size(), ImVec2(-1, 5), "");
    ImGui::TextDisabled("Progress saves automatically.");
    ImGui::SetNextItemWidth(240);
    ImGui::InputTextWithHint("##search", "Search achievements", achievement_search_.data(), achievement_search_.size());
    ImGui::SameLine();
    ImGui::Checkbox("Unlocked only", &unlocked_only_);
    ImGui::Spacing();
    ImGuiTextFilter filter(achievement_search_.data());
    for (const auto& achievement : list) {
      const bool obtained = achievements_->IsUnlocked(achievement.id);
      if ((unlocked_only_ && !obtained) || !filter.PassFilter(achievement.label.c_str())) continue;
      ImGui::PushID(static_cast<int>(achievement.id));
      if (auto* icon = icons_.GetIcon(achievement)) {
        const float tint = obtained ? 1.0f : 0.45f;
        ImGui::ImageWithBg(reinterpret_cast<ImTextureID>(icon), ImVec2(48, 48),
                          ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ImVec4(tint, tint, tint, 1));
      } else ImGui::Dummy(ImVec2(48, 48));
      ImGui::SameLine();
      ImGui::BeginGroup();
      ImGui::TextColored(obtained ? kAccent : ImVec4(0.8f, 0.83f, 0.88f, 1), "%s", achievement.label.c_str());
      ImGui::TextDisabled("%s  /  %u G", obtained ? "Unlocked" : "Locked", achievement.gamerscore);
      ImGui::PushTextWrapPos(ImGui::GetWindowContentRegionMax().x);
      ImGui::TextUnformatted((obtained ? achievement.description : achievement.unachieved_description).c_str());
      ImGui::PopTextWrapPos();
      ImGui::EndGroup();
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::PopID();
    }
    if (list.empty()) ImGui::TextWrapped("No achievement metadata is available for this title yet.");
  }

  void OnDraw(ImGuiIO& io) override {
    const float ui_scale=rex::cvar::Query<int32_t>("iu_ui_scale")/100.0f;
    ImGui::PushFont(nullptr,18.0f*ui_scale);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.055f, 0.09f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.045f, 0.068f, 0.105f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.94f, 0.97f, 1));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.56f, 0.64f, 0.73f, 1));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.17f, 0.26f, 0.36f, 1));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.15f, 0.22f, 1));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.15f, 0.24f, 0.34f, 1));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.18f, 0.3f, 0.42f, 1));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.23f, 0.33f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.37f, 0.5f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.43f, 0.58f, 1));
    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.08f, 0.13f, 0.20f, 1));
    ImGui::PushStyleColor(ImGuiCol_TabSelected, ImVec4(0.17f, 0.29f, 0.40f, 1));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.2f, 0.37f, 0.5f, 1));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, kAccent);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, kAccent);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, kAccent);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 9));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(860.0f*ui_scale, io.DisplaySize.x - 24),
                                  std::min(680.0f*ui_scale, io.DisplaySize.y - 24)), ImGuiCond_Always);
    if(first_draw_) { ImGui::SetNextWindowFocus(); first_draw_=false; }
    if (ImGui::Begin("Infinite Undiscovery##game_menu", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
      ImGui::TextColored(kAccent, "INFINITE UNDISCOVERY");
      ImGui::SameLine();
      ImGui::TextDisabled("RECOMP");
      ImGui::TextDisabled("%s  /  %s", profile_.c_str(), iu::kProjectVersion);
      ImGui::Spacing();
      const float footer_height = ImGui::GetFrameHeightWithSpacing() * 2 +
                                  ImGui::GetTextLineHeightWithSpacing() * (iu::saves::activity.Busy()?3:2) + 24;
      const float body_height = std::max(60.0f, ImGui::GetContentRegionAvail().y - footer_height);
      if (ImGui::BeginTabBar("##sections", ImGuiTabBarFlags_FittingPolicyResizeDown)) {
        const char* tabs[] = {"Settings", "Controls", "Achievements", "Game"};
        for (int tab = 0; tab < 4; ++tab) {
          const auto flags = requested_tab_ == tab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
          if (ImGui::BeginTabItem(tabs[tab], nullptr, flags)) {
            if (requested_tab_ == tab) requested_tab_ = -1;
            active_tab_ = tab;
            ImGui::BeginChild("##body", ImVec2(0, body_height), ImGuiChildFlags_AlwaysUseWindowPadding);
            if (tab == 0) {
              ImGui::TextDisabled("* Save and restart to apply. Hover a label for details.");
              Group("Display", 0); Group("Graphics", 1); Group("Audio", 2);
            } else if (tab == 1) {
              ImGui::TextWrapped("Back + Start opens or closes this menu. Use the D-pad or left stick to navigate, A to select, and B to go back. Menu controls always use the original button layout.");
              Group("Controller",5);
              ImGui::SeparatorText("Controller mapping");
              ImGui::TextWrapped("Choose the physical button for each game action. Trigger-to-trigger mappings retain analog input; buttons activate at half trigger travel.");
              auto mapping=iu::player::GetControllerOptions().mapping;
              if(ImGui::BeginTable("##pad_mapping",2,ImGuiTableFlags_SizingStretchSame)) {
                for(int action=0;action<16;++action) {
                  ImGui::PushID(action);
                  DisplayRow(iu::player::kButtons[action],"Physical input assigned to this in-game action.");
                  if(ImGui::BeginCombo("##source",mapping[action]<0?"Unassigned":iu::player::kButtons[mapping[action]])) {
                    for(int source=-1;source<16;++source) {
                      if(ImGui::Selectable(source<0?"Unassigned":iu::player::kButtons[source],mapping[action]==source)) {
                        mapping[action]=source; Set("iu_pad_mapping",iu::player::MappingString(mapping));
                      }
                    }
                    ImGui::EndCombo();
                  }
                  ImGui::PopID();
                }
                ImGui::EndTable();
              }
              Group("Keyboard & mouse", 3);
              ImGui::SeparatorText("Keyboard bindings");
              ImGui::TextWrapped("Click a binding to edit. Use key names such as Space or Shift+Up; separate alternatives with commas. Click away to apply. Leave blank to unassign.");
              ImGui::BeginDisabled(rex::cvar::GetFlagByName("mnk_mode") != "true");
              Group("Controller buttons", 4);
              ImGui::EndDisabled();
              ImGui::SeparatorText("Menu shortcuts");
              ImGui::TextWrapped("F4  Open / close menu\nF7  Achievements tab\nF10  Profile manager confirmation\nF12  Quit confirmation");
            } else if (tab == 2) DrawAchievements();
            else {
              ImGui::SeparatorText("Current session");
              ImGui::Text("Content profile: %s", profile_.c_str());
              ImGui::TextWrapped("Opening this menu pauses gameplay at the next frame boundary. Closing it resumes play. Background music continues.");
              Group("New game", 7);
              ImGui::TextWrapped("Unlock Hard and Infinity for new games. Existing saves keep their current difficulty. Reopen New Game's difficulty selection after changing this option; no restart is needed.");
              ImGui::TextDisabled("Use Save settings to keep this option for the current content profile.");
              ImGui::SeparatorText("Save game");
              auto save_state=iu::save_anywhere::requests.Get().availability;
              if(iu::saves::activity.Busy() || iu::saves::activity.ExitReserved())
                save_state=iu::save_anywhere::Availability::Saving;
              const bool save_ready=save_state==iu::save_anywhere::Availability::Ready && iu::player::menu_pause.IsPaused();
              ImGui::BeginDisabled(!save_ready);
              if(ImGui::Button("Save Anywhere")) {
                if(iu::save_anywhere::requests.Request()) close_();
              }
              ImGui::EndDisabled();
              ImGui::TextWrapped("%s",iu::save_anywhere::Explain(save_state));
              ImGui::TextWrapped("Use during normal exploration. Weapons must be sheathed and the party must be out of combat. Unavailable during cutscenes, scripted events, transitions, dialogue or other game menus.");
              ImGui::TextDisabled("Choose a slot in the game's save screen. This menu will close.");
              DrawBackups();
              ImGui::Spacing();
              ImGui::SeparatorText("Content & profiles");
              ImGui::TextWrapped("Manage installed discs, DLC, language and content profiles in the Profile Manager. Returning there ends the current game session; save your game first.");
              ImGui::BeginDisabled(iu::saves::activity.Busy());
              if (ImGui::Button("Open Profile Manager...")) { confirm_profiles_=true; ImGui::OpenPopup("End game session?"); }
              ImGui::EndDisabled();
              ImGui::Spacing();
              ImGui::SeparatorText("Exit");
              ImGui::TextWrapped("Quit to desktop. Unsaved game progress will be lost.");
              ImGui::BeginDisabled(iu::saves::activity.Busy());
              if (ImGui::Button("Quit game...")) { confirm_profiles_=false; ImGui::OpenPopup("End game session?"); }
              ImGui::EndDisabled();
              if(ImGui::BeginPopupModal("End game session?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Unsaved game progress will be lost.");
                if(iu::saves::activity.Busy())ImGui::TextUnformatted(iu::saves::kBusyMessage);
                if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                ImGui::SetItemDefaultFocus(); ImGui::SameLine();
                ImGui::BeginDisabled(iu::saves::activity.Busy());
                if(ImGui::Button(confirm_profiles_?"Return to Profile Manager":"Quit to desktop")) {
                  if(confirm_profiles_) profiles_(); else quit_();
                  ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
                ImGui::EndPopup();
              }
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
          }
        }
        ImGui::EndTabBar();
      }
      ImGui::Separator();
      if(iu::saves::activity.Busy()) {
        ImGui::TextColored(kAccent,"%s",iu::saves::kBusyMessage);
        ImGui::TextDisabled("The game continues while save data is in use; menu pause resumes afterward.");
      }
      else if (!display_.Error().empty()) {
        ImGui::TextColored(ImVec4(1,0.65f,0.5f,1),"Display change failed. Hover for details.");
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",display_.Error().c_str());
      }
      else if (!message_.empty()) ImGui::TextColored(save_failed_ ? ImVec4(1, 0.65f, 0.5f, 1) : kAccent, "%s", message_.c_str());
      else if (settings_.Unsaved()) ImGui::TextColored(kAccent, "Unsaved settings. Save to keep changes for the next launch.");
      else ImGui::TextDisabled(iu::player::menu_pause.IsPaused() ? "Game paused. F4 closes the menu and resumes play." : "Waiting for the game to reach a pause point. F4 closes the menu.");
      if (settings_.NeedsRestart()) ImGui::TextColored(kAccent, "Restart required for changed display / graphics settings.");
      else ImGui::TextDisabled(" ");

      if (ImGui::Button("Save settings")) SaveSettings();
      if (save_failed_ && !save_error_.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", save_error_.c_str());
      ImGui::SameLine();
      if(settings_.NeedsRestart()) {
        ImGui::BeginDisabled(iu::saves::activity.Busy());
        if(ImGui::Button("Save settings and restart...")) { restore_selection_={};ImGui::OpenPopup("Restart game?"); }
        ImGui::EndDisabled();
        ImGui::SameLine();
      }
      if (active_tab_ <= 1) {
        if (ImGui::Button("Reset this tab")) ImGui::OpenPopup("Reset settings?");
        ImGui::SameLine();
      }
      if (ImGui::Button("Return to game")) close_();
      if(request_restore_confirmation_) {request_restore_confirmation_=false;ImGui::OpenPopup("Restart game?");}
      if(ImGui::BeginPopupModal("Restart game?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        const bool restore=!restore_selection_.id.empty();
        if(restore) {
          ImGui::Text("Restore %s",restore_selection_.slot.c_str());
          ImGui::Text("Backup from %s (local time)",restore_selection_.created.c_str());
          ImGui::TextUnformatted("This replaces this slot's saved progress. A recovery backup is kept first.");
        }
        ImGui::TextUnformatted("Settings will be saved, then the game will restart in this profile.");
        ImGui::TextUnformatted("This does NOT save game progress. Unsaved progress will be lost.");
        ImGui::TextUnformatted("Save in the game first. Restart is unavailable while save data is in use.");
        if(iu::saves::activity.Busy())ImGui::TextUnformatted(iu::saves::kBusyMessage);
        if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
        ImGui::SetItemDefaultFocus();ImGui::SameLine();
        ImGui::BeginDisabled(iu::saves::activity.Busy());
        if(ImGui::Button(restore?"Restore and restart":"Save settings and restart")) {
          if(SaveSettings()) {
            std::string error;
            if(!restart_(restore_selection_.id,error)) {save_failed_=true;save_error_=error;message_="Restart failed. Hover Save settings for details.";}
            else message_="Restarting game...";
          }
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
      }
      if (ImGui::BeginPopupModal("Reset settings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Restore this tab's defaults for the current session?");
        ImGui::TextDisabled("Use Save settings afterward to keep the defaults.");
        if (ImGui::Button("Restore defaults")) { ResetTab(active_tab_ == 1); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
      }
    }
    ImGui::End();
    ImGui::PopStyleVar(6);
    ImGui::PopStyleColor(17);
    ImGui::PopFont();
  }

  rex::system::AchievementManager* achievements_;
  rex::ui::AchievementIconCache icons_;
  MenuSettings& settings_;
  std::filesystem::path config_;
  std::string profile_;
  std::function<void()> close_, profiles_, quit_;
  int requested_tab_ = -1, active_tab_ = 0;
  bool unlocked_only_ = false, save_failed_ = false;
  std::array<char, 128> achievement_search_{};
  std::map<std::string, std::array<char, 256>> binding_buffers_;
  std::string message_, save_error_;
  DisplayWindow& display_;
  std::function<void(std::function<void()>)> apply_display_change_;
  bool confirm_profiles_=false;
  bool first_draw_=true;
  std::shared_ptr<iu::saves::Backups> backups_;
  std::function<bool(const std::string&,std::string&)> restart_;
  std::vector<iu::saves::Backup> backup_list_;
  iu::saves::Backup restore_selection_;
  bool request_restore_confirmation_=false;
};
}  // namespace iu::ui
