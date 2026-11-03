#include <rex/system/xam/content_manager.h>
#include <rex/logging.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <windows.h>

namespace fs = std::filesystem;
using rex::X_RESULT;
using namespace rex::system;
using namespace rex::system::xam;
static void Check(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
int main() {
  try {
    rex::InitLogging(nullptr, spdlog::level::warn);
    const auto root = fs::current_path() / ("content-header-test-" + std::to_string(GetCurrentProcessId()));
    Check(!fs::exists(root), "Test directory already exists");
    const auto title_root = root / "B13EBABEBABEBABE" / "535107DB";
    const auto packages = title_root / "00000001";
    const auto headers = title_root / "Headers" / "00000001";
    fs::create_directories(packages / "InfiniteUndiscovery_0002.bin");
    fs::create_directories(packages / "truncated");
    fs::create_directories(packages / "valid");
    fs::create_directories(headers);
    std::ofstream(headers / "truncated.header", std::ios::binary).put('x');
    constexpr uint64_t xuid = 0xB13EBABEBABEBABEull;
    constexpr uint32_t title = 0x535107DB;
    // Explicit title and XUID keep this test independent of a running guest.
    ContentManager manager(nullptr, root);
    XCONTENT_AGGREGATE_DATA valid{};
    valid.device_id = 1; valid.content_type = XContentType::kSavedGame;
    valid.xuid = xuid; valid.title_id = title;
    valid.set_file_name("valid"); valid.set_display_name(u"Existing header title");
    Check(manager.WriteContentHeaderFile(xuid, valid, 0) == X_ERROR_SUCCESS, "Write valid header");
    const auto list = manager.ListContent(1, xuid, XContentType::kSavedGame, title);
    Check(list.size() == 3, "All save directories must be enumerated");
    bool missing_seen = false, truncated_seen = false, valid_seen = false;
    for (const auto& item : list) {
      // Bound reads even on the buggy SDK, where these bytes are uninitialized.
      Check(std::memchr(item.file_name_raw, 0, sizeof(item.file_name_raw)) != nullptr, "Package name must terminate");
      const auto name = item.file_name();
      Check(item.device_id == 1 && item.content_type == XContentType::kSavedGame &&
            item.xuid == xuid && item.title_id == title, "Save metadata must be initialized");
      if (name == "InfiniteUndiscovery_0002.bin") {
        missing_seen = true;
        Check(item.display_name() == u"InfiniteUndiscovery_0002.bin", "Missing header must use folder display name");
      } else if (name == "truncated") {
        truncated_seen = true;
        Check(item.display_name() == u"truncated", "Truncated header must fall back to directory metadata");
      } else if (name == "valid") {
        valid_seen = true;
        Check(item.display_name() == u"Existing header title", "Valid header must be preserved");
      } else throw std::runtime_error("Unexpected package name in enumeration");
    }
    Check(missing_seen && truncated_seen && valid_seen, "Each save must retain its identity");
    std::cout << "PASS missing, truncated, and valid content headers\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
