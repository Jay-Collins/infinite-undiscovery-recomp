#pragma once
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace iu::saves {
struct Backup {
  std::string id, slot, account, created;
  uint64_t bytes = 0;
  uint64_t timestamp = 0;
  bool can_restore = true;
};
class Backups {
 public:
  explicit Backups(std::filesystem::path profile);
  // Call before guest launch. Resolves an interrupted restore first.
  void Initialize();
  void CaptureAll();
  void CapturePackage(const std::filesystem::path& package);
  std::vector<Backup> List();
  bool Validate(const std::string& id, std::string& error);
  // Startup only: caller must hold the profile session lock, with no guest alive.
  bool Restore(const std::string& id, std::string& error);
  std::string Status();
  // Deterministic filesystem-failure injection for restore recovery tests.
  std::function<void(int)> restore_checkpoint;
 private:
  std::filesystem::path profile_, saves_, store_;
  std::mutex mutex_;
  std::string status_;
  void Capture(const std::filesystem::path& package, bool prune = true);
  void Recover();
};
void SetActive(std::shared_ptr<Backups> backups);
std::shared_ptr<Backups> Active();
} // namespace iu::saves
