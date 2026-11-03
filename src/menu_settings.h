#pragma once

#include <rex/cvar.h>
#include <toml++/toml.hpp>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <windows.h>

namespace iu::ui {

// Only player-facing settings belong in the menu. Keep engine/debug variables
// and portable paths out of both the UI and its persistence operation.
struct MenuSetting {
  const char* name;
  const char* label;
  const char* help;
  int group;  // -1 custom display widgets; 0 display, 1 graphics, 2 audio, 3 controls, 4 bindings
};
inline constexpr MenuSetting kMenuSettings[] = {
    {"fullscreen", "Fullscreen", "Borderless fullscreen uses the desktop resolution.", 0},
    {"monitor", "", "", -1},
    {"iu_ui_scale", "UI scale", "Size of the overlay text and controls.", 0},
    {"iu_performance", "", "", -1},
    {"iu_master_volume", "Master volume", "Volume of all game audio, from 0 to 100 percent.", 2},
    {"iu_left_deadzone", "Left stick deadzone", "Radial deadzone percentage. 0 preserves the original input.", 5},
    {"iu_right_deadzone", "Right stick deadzone", "Radial deadzone percentage. Increase to suppress camera drift.", 5},
    {"iu_vibration", "Vibration strength", "Controller motor strength. 0 disables vibration.", 5},
    {"iu_swap_sticks", "Swap sticks", "Swap movement and camera controls.", 5},
    {"iu_invert_camera_y", "Invert camera Y", "Reverse vertical input on the camera stick.", 5},
    {"iu_pad_mapping", "", "", 6},
    {"window_width", "", "", -1},
    {"window_height", "", "", -1},
    {"present_letterbox", "", "", -1},
    {"vsync", "Vertical sync", "Synchronize presentation. Disabling this can affect game timing.", 1},
    {"iu_frame_rate", "Frame rate", "60 FPS is experimental. Keep VSync enabled. First-use shader or asset loading hitches may remain. Save and restart to apply.", 1},
    {"resolution_scale", "", "", -1},
    {"draw_resolution_scale_x", "", "", -1},
    {"draw_resolution_scale_y", "", "", -1},
    {"swap_post_effect", "Anti-aliasing", "Post-process smoothing: none, FXAA, or stronger FXAA. Restart required.", 1},
    {"present_dither", "Color dithering", "Reduce visible color banding. Restart required.", 1},
    {"audio_mute", "Mute audio", "Mute all game audio. Individual volume controls are in the game's own options.", 2},
    {"mnk_mode", "Keyboard and mouse controls", "Emulate a controller using the bindings below.", 3},
    {"mnk_mouse", "Mouse camera", "Use mouse movement for the right stick when keyboard controls are enabled.", 3},
    {"mnk_sensitivity", "Mouse sensitivity", "Sensitivity of the emulated right stick.", 3},
    {"keybind_a", "A / Examine", "", 4},
    {"keybind_b", "B / Cancel", "", 4},
    {"keybind_x", "X", "", 4},
    {"keybind_y", "Y", "", 4},
    {"keybind_left_trigger", "Left trigger", "", 4},
    {"keybind_right_trigger", "Right trigger / Sheathe", "", 4},
    {"keybind_left_shoulder", "Left shoulder", "", 4},
    {"keybind_right_shoulder", "Right shoulder", "", 4},
    {"keybind_lstick_up", "Move forward", "", 4},
    {"keybind_lstick_down", "Move backward", "", 4},
    {"keybind_lstick_left", "Move left", "", 4},
    {"keybind_lstick_right", "Move right", "", 4},
    {"keybind_lstick_press", "Left stick click", "", 4},
    {"keybind_rstick_up", "Camera up", "", 4},
    {"keybind_rstick_down", "Camera down", "", 4},
    {"keybind_rstick_left", "Camera left", "", 4},
    {"keybind_rstick_right", "Camera right", "", 4},
    {"keybind_rstick_press", "Right stick click", "", 4},
    {"keybind_dpad_up", "D-pad up", "", 4},
    {"keybind_dpad_down", "D-pad down", "", 4},
    {"keybind_dpad_left", "D-pad left", "", 4},
    {"keybind_dpad_right", "D-pad right", "", 4},
    {"keybind_start", "Start", "", 4},
    {"keybind_back", "Back", "", 4},
};

class MenuSettings {
 public:
  void CaptureStartup() {
    saved_.clear();
    for (const auto& setting : kMenuSettings) {
      if (const auto* flag = rex::cvar::GetFlagInfo(setting.name))
        saved_[setting.name] = flag->getter();
    }
    startup_ = saved_;
  }

  bool Unsaved() const { return Changed(saved_, false); }
  bool NeedsRestart() const { return Changed(startup_, true); }
  void MarkDisplayApplied() {
    for(const char* name:{"fullscreen","monitor","window_width","window_height","present_letterbox"})
      startup_[name]=rex::cvar::GetFlagByName(name);
  }

  // Merge only our fields, retaining hand-edited engine options. A TOML writer
  // escapes Windows paths correctly. Defaults are written too, so resetting a
  // value cannot leave a stale override in the file.
  bool Save(const std::filesystem::path& path, std::string& error) {
    auto temporary = path;
    temporary += ".menu-tmp";
    try {
      toml::table table;
      if (std::filesystem::exists(path)) table = toml::parse_file(path.string());
      for (const auto& setting : kMenuSettings) {
        const auto* flag = rex::cvar::GetFlagInfo(setting.name);
        if (!flag) continue;
        const auto value = flag->getter();
        switch (flag->type) {
          case rex::cvar::FlagType::Boolean:
            table.insert_or_assign(setting.name, value == "true"); break;
          case rex::cvar::FlagType::Int32:
            table.insert_or_assign(setting.name, std::stoll(value)); break;
          case rex::cvar::FlagType::Double:
            table.insert_or_assign(setting.name, std::stod(value)); break;
          case rex::cvar::FlagType::String:
            table.insert_or_assign(setting.name, value); break;
          default: throw std::runtime_error("Unsupported menu setting type");
        }
      }
      std::filesystem::create_directories(path.parent_path());
      {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output << "# Infinite Undiscovery player settings\n" << table << '\n';
        output.flush();
        output.close();
      }
      if (!MoveFileExW(temporary.c_str(), path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::system_error(GetLastError(), std::system_category(), "Replacing settings file");
      for (auto& [name, value] : saved_) value = rex::cvar::GetFlagByName(name);
      error.clear();
      return true;
    } catch (const std::exception& e) {
      error = e.what();
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
      return false;
    }
  }

 private:
  bool Changed(const std::map<std::string, std::string>& baseline, bool restart_only) const {
    for (const auto& [name, value] : baseline) {
      const auto* flag = rex::cvar::GetFlagInfo(name);
      if (flag && (!restart_only || flag->lifecycle == rex::cvar::Lifecycle::kRequiresRestart) &&
          flag->getter() != value) return true;
    }
    return false;
  }
  std::map<std::string, std::string> saved_, startup_;
};
}  // namespace iu::ui
