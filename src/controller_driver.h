#pragma once
#include "player_options.h"
#include <rex/input/input_driver.h>
#include <map>
#include <cstring>
#include <atomic>

namespace iu::player {
using rex::X_RESULT;
// Wrap physical devices only. Keyboard emulation retains its own bindings.
// Raw input remains available to the host menu while guest input is blocked.
class ControllerDriver final : public rex::input::InputDriver {
 public:
  explicit ControllerDriver(std::unique_ptr<rex::input::InputDriver> driver)
      : InputDriver(nullptr,0), driver_(std::move(driver)) {}
  rex::X_STATUS Setup() override { return driver_->Setup(); }
  void OnWindowAvailable(rex::ui::Window* window) override { driver_->OnWindowAvailable(window); }
  void EnumerateDevices(std::vector<rex::input::DeviceInfo>& out) override {
    std::lock_guard lock(mutex_); driver_->EnumerateDevices(out);
  }
  rex::X_RESULT GetDeviceCapabilities(rex::input::DeviceId id, uint32_t flags, rex::input::X_INPUT_CAPABILITIES* caps) override {
    std::lock_guard lock(mutex_); return driver_->GetDeviceCapabilities(id,flags,caps);
  }
  rex::input::X_INPUT_GAMEPAD PollRaw() {
    std::lock_guard lock(mutex_);
    std::vector<rex::input::DeviceInfo> devices;
    driver_->EnumerateDevices(devices);
    for (const auto& device : devices) {
      rex::input::X_INPUT_STATE state{};
      if (driver_->GetDeviceState(device.id,&state) == X_ERROR_SUCCESS) return state.gamepad;
    }
    return {};
  }
  void SuppressUntilReleased() { suppress_until_release_.store(true); }
  rex::X_RESULT GetDeviceState(rex::input::DeviceId id, rex::input::X_INPUT_STATE* out) override {
    std::lock_guard lock(mutex_);
    rex::input::X_INPUT_STATE raw{};
    const auto result=driver_->GetDeviceState(id,&raw);
    if (result != X_ERROR_SUCCESS) return result;
    auto gamepad=Transform(raw.gamepad,GetControllerOptions());
    const bool chord=(uint16_t(raw.gamepad.buttons)&0x30)==0x30;
    const bool release_gate=suppress_until_release_.load();
    if(release_gate && uint16_t(raw.gamepad.buttons)==0 && raw.gamepad.left_trigger==0 && raw.gamepad.right_trigger==0)
      suppress_until_release_.store(false);
    if (!is_active() || chord || release_gate) gamepad={};
    auto& previous=states_[id];
    if (std::memcmp(&previous.gamepad,&gamepad,sizeof(gamepad))) {
      previous.gamepad=gamepad; previous.packet_number=uint32_t(previous.packet_number)+1;
    }
    if (out) *out=previous;
    return result;
  }
  rex::X_RESULT SetDeviceVibration(rex::input::DeviceId id, rex::input::X_INPUT_VIBRATION* value) override {
    if (!value) return X_ERROR_BAD_ARGUMENTS;
    std::lock_guard lock(mutex_);
    auto scaled=ScaleVibration(*value,is_active()?GetControllerOptions().vibration:0);
    return driver_->SetDeviceVibration(id,&scaled);
  }
  void StopRumble() {
    std::lock_guard lock(mutex_);
    std::vector<rex::input::DeviceInfo> devices; driver_->EnumerateDevices(devices);
    rex::input::X_INPUT_VIBRATION zero{};
    for (const auto& device:devices) driver_->SetDeviceVibration(device.id,&zero);
  }
  rex::X_RESULT GetDeviceKeystroke(rex::input::DeviceId id, uint32_t flags, rex::input::X_INPUT_KEYSTROKE* out) override {
    // This title imports GetState, not GetKeystroke. Preserve the backend's
    // keystroke API for SDK callers, but never leak keys through the overlay.
    std::lock_guard lock(mutex_);
    if (!is_active()) return X_ERROR_EMPTY;
    return driver_->GetDeviceKeystroke(id,flags,out);
  }
 private:
  std::unique_ptr<rex::input::InputDriver> driver_;
  std::mutex mutex_;
  std::map<rex::input::DeviceId,rex::input::X_INPUT_STATE> states_;
  std::atomic<bool> suppress_until_release_{false};
};
} // namespace iu::player
