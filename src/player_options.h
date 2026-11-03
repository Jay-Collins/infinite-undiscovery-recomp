#pragma once
#include <rex/cvar.h>
#include <rex/input/input.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <mutex>
#include <string>

namespace iu::player {
inline constexpr const char* kDefaultMapping = "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15";
inline constexpr const char* kButtons[] = {"D-pad up", "D-pad down", "D-pad left", "D-pad right",
    "Start", "Back", "Left stick click", "Right stick click", "LB", "RB", "A", "B", "X", "Y", "LT", "RT"};
inline constexpr uint16_t kMasks[] = {1,2,4,8,16,32,64,128,256,512,4096,8192,16384,32768};
struct ControllerOptions {
  std::array<int, 16> mapping{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  int left_deadzone = 0, right_deadzone = 0, vibration = 100;
  bool swap_sticks = false, invert_right_y = false;
};
inline bool ParseMapping(std::string_view text, std::array<int, 16>& result) {
  std::array<int,16> parsed{};
  for (size_t i=0; i<parsed.size(); ++i) {
    const auto comma = text.find(',');
    const auto token = text.substr(0, comma);
    const auto [end, ec] = std::from_chars(token.data(), token.data()+token.size(), parsed[i]);
    if (ec != std::errc{} || end != token.data()+token.size() || parsed[i] < -1 || parsed[i] > 15) return false;
    if (i == 15) { if (comma != std::string_view::npos) return false; }
    else { if (comma == std::string_view::npos) return false; text.remove_prefix(comma+1); }
  }
  result = parsed;
  return true;
}
inline std::string MappingString(const std::array<int,16>& mapping) {
  std::string result;
  for (size_t i=0;i<mapping.size();++i) { if(i) result += ','; result += std::to_string(mapping[i]); }
  return result;
}
inline void Deadzone(int16_t& x, int16_t& y, int percent) {
  if (percent <= 0) return; // Preserve original integer endpoints at default.
  const double fx = double(x)/32768.0, fy = double(y)/32768.0;
  const double radius = std::hypot(fx, fy), threshold = std::clamp(percent, 0, 50)/100.0;
  if (radius <= threshold) { x=0; y=0; return; }
  const double gain = std::min(1.0, (radius-threshold)/(1.0-threshold))/radius;
  x = static_cast<int16_t>(std::clamp(std::lround(fx*gain*32768.0), -32768L, 32767L));
  y = static_cast<int16_t>(std::clamp(std::lround(fy*gain*32768.0), -32768L, 32767L));
}
inline rex::input::X_INPUT_GAMEPAD Transform(const rex::input::X_INPUT_GAMEPAD& raw, const ControllerOptions& options) {
  auto result = raw;
  const auto strength = [&](int source) -> uint8_t {
    if (source < 0) return 0;
    if (source == 14) return raw.left_trigger;
    if (source == 15) return raw.right_trigger;
    return (uint16_t(raw.buttons) & kMasks[source]) ? 255 : 0;
  };
  uint16_t buttons = uint16_t(raw.buttons) & 0x0400; // Preserve Guide.
  for (int i=0; i<14; ++i) if (strength(options.mapping[i]) >= 128) buttons |= kMasks[i];
  result.buttons = buttons;
  result.left_trigger = strength(options.mapping[14]);
  result.right_trigger = strength(options.mapping[15]);
  int16_t lx=raw.thumb_lx, ly=raw.thumb_ly, rx=raw.thumb_rx, ry=raw.thumb_ry;
  Deadzone(lx, ly, options.left_deadzone); Deadzone(rx, ry, options.right_deadzone);
  if (options.swap_sticks) { std::swap(lx,rx); std::swap(ly,ry); }
  if (options.invert_right_y) ry = static_cast<int16_t>(std::clamp(-int(ry), -32768, 32767));
  result.thumb_lx=lx; result.thumb_ly=ly; result.thumb_rx=rx; result.thumb_ry=ry;
  return result;
}
inline rex::input::X_INPUT_VIBRATION ScaleVibration(const rex::input::X_INPUT_VIBRATION& value, int percent) {
  rex::input::X_INPUT_VIBRATION result{};
  result.left_motor_speed = uint32_t(uint16_t(value.left_motor_speed))*std::clamp(percent,0,100)/100;
  result.right_motor_speed = uint32_t(uint16_t(value.right_motor_speed))*std::clamp(percent,0,100)/100;
  return result;
}
inline std::mutex options_mutex;
inline ControllerOptions controller_options;
inline ControllerOptions GetControllerOptions() { std::lock_guard lock(options_mutex); return controller_options; }
void InitializeOptions();
} // namespace iu::player
