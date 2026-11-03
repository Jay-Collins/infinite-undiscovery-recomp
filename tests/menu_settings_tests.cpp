#include "../src/menu_settings.h"
#include "../src/display_settings.h"
#include <rex/logging.h>
#include <iostream>

static void Check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
static std::string Read(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), {});
}

int main() {
  try {
    rex::InitLogging(nullptr, spdlog::level::warn);
    // Load the real GPU plugin's cvar registrations, including its shared and
    // per-axis scaling precedence. No graphics device or game data is needed.
    const auto gpu = std::filesystem::current_path() / "rexgpu-xenos.dll";
    Check(LoadLibraryW(gpu.c_str()) != nullptr, "Load GPU settings for integration tests");
    const auto root = std::filesystem::current_path() /
        ("menu-settings-test-" + std::to_string(GetCurrentProcessId()));
    Check(!std::filesystem::exists(root), "Test directory must be fresh");
    const auto path = root / "profile" / "runtime.toml";
    std::filesystem::create_directories(path.parent_path());
    const std::string windows_path = "C:\\Games\\Infinite Undiscovery\\USA\\saves";
    toml::table original;
    original.insert("user_data_root", windows_path);
    original.insert("custom_engine_option", 42);
    std::ofstream(path) << original;

    Check(rex::cvar::GetFlagInfo("fullscreen") != nullptr, "Window flags available");
    Check(rex::cvar::SetFlagByName("fullscreen", "true"), "Set fullscreen baseline");
    Check(rex::cvar::SetFlagByName("window_width", "0"), "Set window baseline");
    iu::ui::MenuSettings settings;
    settings.CaptureStartup();
    Check(!settings.Unsaved() && !settings.NeedsRestart(), "Clean initial state");
    Check(rex::cvar::SetFlagByName("fullscreen", "false"), "Change live setting");
    Check(settings.Unsaved() && !settings.NeedsRestart(), "Live changes need saving but not restart");
    Check(rex::cvar::SetFlagByName("window_width", "1280"), "Change restart setting");
    Check(settings.NeedsRestart(), "Restart change tracked");
    std::string error;
    Check(settings.Save(path, error), error.c_str());
    Check(!settings.Unsaved() && settings.NeedsRestart(), "Saving must not clear restart state");
    auto saved = toml::parse_file(path.string());
    Check(saved["fullscreen"].value<bool>() == false, "Boolean round-trip");
    Check(saved["window_width"].value<int>() == 1280, "Integer round-trip");
    Check(saved["user_data_root"].value<std::string>() == windows_path, "Retain and escape Windows paths");
    Check(saved["custom_engine_option"].value<int>() == 42, "Retain unknown manual options");
    Check(!std::filesystem::exists(path.string() + ".menu-tmp"), "No leftover temporary file");

    Check(rex::cvar::SetFlagByName("window_width", "0"), "Restore default");
    Check(settings.Save(path, error), error.c_str());
    saved = toml::parse_file(path.string());
    Check(saved["window_width"].value<int>() == 0, "Defaults replace stale persisted overrides");
    Check(!settings.NeedsRestart(), "Restoring startup value clears restart notice");

    Check(rex::cvar::GetFlagByName("iu_frame_rate") == "30", "Original frame rate is the default");
    Check(!rex::cvar::SetFlagByName("iu_frame_rate", "45"), "Reject unsupported frame divisors");
    Check(rex::cvar::SetFlagByName("iu_frame_rate", "60"), "Select experimental frame rate");
    Check(settings.NeedsRestart(), "Frame rate needs a restart");
    Check(settings.Save(path, error), error.c_str());
    saved = toml::parse_file(path.string());
    Check(saved["iu_frame_rate"].value<std::string>() == "60", "Persist frame rate selection");
    Check(settings.NeedsRestart(), "Saving frame rate does not apply it to running game");
    Check(rex::cvar::SetFlagByName("iu_frame_rate", "30"), "Restore original frame rate");

    std::ofstream(path, std::ios::trunc) << "invalid = [\n";
    const auto damaged = Read(path);
    Check(rex::cvar::SetFlagByName("fullscreen", "true"), "Unsaved edit before failure");
    Check(!settings.Save(path, error) && !error.empty(), "Malformed config must report failure");
    Check(Read(path) == damaged, "Failed save must preserve existing file");
    Check(settings.Unsaved(), "Failed save must retain dirty state");

    const auto blocker = root / "not-a-directory";
    std::ofstream(blocker) << "keep";
    Check(!settings.Save(blocker / "runtime.toml", error), "Filesystem failure reported");
    Check(Read(blocker) == "keep", "Filesystem failure preserves blocker");

    const auto fresh = root / "new-profile" / "runtime.toml";
    Check(settings.Save(fresh, error), error.c_str());
    Check(toml::parse_file(fresh.string())["fullscreen"].value<bool>() == true, "Create config and parents");

    Check(rex::cvar::SetFlagByName("resolution_scale", "3"), "Legacy shared scale");
    Check(rex::cvar::SetFlagByName("draw_resolution_scale_x", "1"), "Legacy default X");
    Check(rex::cvar::SetFlagByName("draw_resolution_scale_y", "2"), "Legacy override Y");
    Check(iu::ui::EffectiveInternalScale() == std::pair{3, 2}, "Show effective legacy axis overrides");
    Check(iu::ui::SetInternalScale(4), "Apply uniform internal resolution");
    Check(iu::ui::EffectiveInternalScale() == std::pair{4, 4}, "Uniform preset clears mismatched axes");
    Check(iu::ui::SetDisplayResolution(1920, 1080), "Apply display resolution preset");
    Check(rex::cvar::GetFlagByName("video_mode_width") == "1280" &&
          rex::cvar::GetFlagByName("video_mode_height") == "720", "Output size must not change guest video mode");
    Check(settings.Save(fresh, error), error.c_str());
    saved = toml::parse_file(fresh.string());
    Check(saved["window_width"].value<int>() == 1920 && saved["window_height"].value<int>() == 1080,
          "Display preset persists both dimensions");
    Check(saved["resolution_scale"].value<int>() == 4 && saved["draw_resolution_scale_x"].value<int>() == 4 &&
          saved["draw_resolution_scale_y"].value<int>() == 4, "Persist shared and per-axis scale together");
    Check(iu::ui::SetInternalScale(1), "Return to native scale");
    Check(iu::ui::EffectiveInternalScale() == std::pair{1, 1}, "Native must clear the shared fallback too");
    Check(!iu::ui::SetInternalScale(0) && !iu::ui::SetInternalScale(9), "Reject unsupported scales");
    Check(iu::ui::EffectiveInternalScale() == std::pair{1, 1}, "Invalid scales must not change configuration");
    Check(!iu::ui::SetDisplayResolution(1920, 0), "Reject incomplete output dimensions");
    Check(rex::cvar::GetFlagByName("window_height") == "1080", "Invalid resolution leaves both dimensions intact");
    std::cout << "PASS settings persistence, defaults, paths, restart state and failure recovery\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
