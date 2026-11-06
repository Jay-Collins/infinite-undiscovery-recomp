#include "player_options.h"
#include <rex/audio/downmix.h>

REXCVAR_DEFINE_INT32(iu_ui_scale, 100, "Player", "Menu scale percentage").range(75, 200);
REXCVAR_DEFINE_INT32(iu_master_volume, 100, "Player", "Master volume percentage").range(0, 100);
REXCVAR_DEFINE_INT32(iu_performance, 0, "Player", "Performance display: 0 off, 1 FPS, 2 FPS and frame time").range(0, 2);
REXCVAR_DEFINE_BOOL(iu_unlock_difficulties, false, "Player", "Make all difficulties available when starting a new game");
REXCVAR_DEFINE_STRING(iu_frame_rate, "30", "Player", "Frame rate target; 60 FPS is experimental. Save and restart to apply.")
    .allowed({"30", "60"}).lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_INT32(iu_left_deadzone, 0, "Player", "Left stick radial deadzone percentage").range(0, 50);
REXCVAR_DEFINE_INT32(iu_right_deadzone, 0, "Player", "Right stick radial deadzone percentage").range(0, 50);
REXCVAR_DEFINE_INT32(iu_vibration, 100, "Player", "Vibration strength percentage").range(0, 100);
REXCVAR_DEFINE_BOOL(iu_swap_sticks, false, "Player", "Swap movement and camera sticks");
REXCVAR_DEFINE_BOOL(iu_invert_camera_y, false, "Player", "Invert camera stick Y");
REXCVAR_DEFINE_STRING(iu_pad_mapping, iu::player::kDefaultMapping, "Player", "Physical source for each controller action");

namespace iu::player {
void InitializeOptions() {
  auto update = [](std::string_view, std::string_view) {
    ControllerOptions options;
    ParseMapping(rex::cvar::GetFlagByName("iu_pad_mapping"), options.mapping);
    options.left_deadzone = rex::cvar::Query<int32_t>("iu_left_deadzone");
    options.right_deadzone = rex::cvar::Query<int32_t>("iu_right_deadzone");
    options.vibration = rex::cvar::Query<int32_t>("iu_vibration");
    options.swap_sticks = rex::cvar::Query<bool>("iu_swap_sticks");
    options.invert_right_y = rex::cvar::Query<bool>("iu_invert_camera_y");
    { std::lock_guard lock(options_mutex); controller_options=options; }
    rex::audio::SetOutputGain(rex::cvar::Query<int32_t>("iu_master_volume")/100.0f);
  };
  for (const char* name : {"iu_pad_mapping", "iu_left_deadzone", "iu_right_deadzone", "iu_vibration",
                           "iu_swap_sticks", "iu_invert_camera_y", "iu_master_volume"})
    rex::cvar::RegisterChangeCallback(name, update);
  update({}, {});
}
} // namespace iu::player
