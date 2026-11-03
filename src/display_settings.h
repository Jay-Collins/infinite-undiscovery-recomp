#pragma once

#include <rex/cvar.h>
#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace iu::ui {

struct DisplayResolution {
  int width, height;
  const char* label;
};
inline constexpr DisplayResolution kDisplayResolutions[] = {
    {0, 0, "Automatic"},
    {1280, 720, "1280 x 720 (720p)"},
    {1600, 900, "1600 x 900 (900p)"},
    {1920, 1080, "1920 x 1080 (1080p)"},
    {2560, 1440, "2560 x 1440 (1440p)"},
    {3840, 2160, "3840 x 2160 (4K)"},
    {1280, 800, "1280 x 800 (16:10)"},
    {1920, 1200, "1920 x 1200 (16:10)"},
    {2560, 1600, "2560 x 1600 (16:10)"},
    {2560, 1080, "2560 x 1080 (ultrawide)"},
    {3440, 1440, "3440 x 1440 (ultrawide)"},
    {5120, 1440, "5120 x 1440 (32:9)"},
};

// The SDK gives non-default per-axis values priority over the shared scale.
// Reflect legacy configurations accurately until a uniform preset is chosen.
inline std::pair<int, int> EffectiveInternalScale() {
  const auto read = [](const char* name) {
    const auto value = rex::cvar::GetFlagByName(name);
    return value.empty() ? 1 : std::clamp(std::stoi(value), 1, 8);
  };
  const int shared = read("resolution_scale");
  const int x = read("draw_resolution_scale_x");
  const int y = read("draw_resolution_scale_y");
  return {x == 1 ? shared : x, y == 1 ? shared : y};
}

template <size_t N>
inline bool SetDisplayValues(const std::array<const char*, N>& names,
                             const std::array<std::string, N>& values) {
  std::array<std::string, N> previous;
  for (size_t i = 0; i < N; ++i) {
    if (!rex::cvar::GetFlagInfo(names[i])) return false;
    previous[i] = rex::cvar::GetFlagByName(names[i]);
  }
  for (size_t i = 0; i < N; ++i) {
    if (!rex::cvar::SetFlagByName(names[i], values[i])) {
      for (size_t j = 0; j < i; ++j) rex::cvar::SetFlagByName(names[j], previous[j]);
      return false;
    }
  }
  return true;
}

inline bool SetDisplayResolution(int width, int height) {
  if ((width == 0) != (height == 0) || width < 0 || height < 0 || width > 8192 || height > 8192)
    return false;
  // Host window size only. Changing the guest video mode is not a substitute
  // for an internal-rendering scale and may change the game's video behavior.
  return SetDisplayValues<2>({"window_width", "window_height"},
                             {std::to_string(width), std::to_string(height)});
}

inline bool SetInternalScale(int scale) {
  if (scale < 1 || scale > 8) return false;
  const auto value = std::to_string(scale);
  // Set all three, including the shared fallback: otherwise choosing 1x could
  // silently retain an old non-default scale from a hand-edited config.
  return SetDisplayValues<3>({"resolution_scale", "draw_resolution_scale_x", "draw_resolution_scale_y"},
                             {value, value, value});
}
}  // namespace iu::ui
