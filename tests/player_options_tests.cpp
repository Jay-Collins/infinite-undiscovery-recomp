#include "../src/controller_driver.h"
#include <rex/logging.h>
#include <rex/audio/downmix.h>
#include <iostream>
#include <stdexcept>

using namespace rex;
using namespace rex::input;
using namespace iu::player;
static void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
class FakePad final : public InputDriver {
 public:
  FakePad():InputDriver(nullptr,0) {}
  X_INPUT_STATE state{};
  X_INPUT_VIBRATION vibration{};
  bool connected=true;
  X_STATUS Setup() override { return 0; }
  void EnumerateDevices(std::vector<DeviceInfo>& out) override { if(connected) out.push_back({static_cast<DeviceId>(1),0,"Test pad","",false}); }
  X_RESULT GetDeviceState(DeviceId,X_INPUT_STATE* out) override { if(!connected) return X_ERROR_DEVICE_NOT_CONNECTED; if(out)*out=state; return 0; }
  X_RESULT SetDeviceVibration(DeviceId,X_INPUT_VIBRATION* value) override { vibration=*value; return 0; }
  X_RESULT GetDeviceCapabilities(DeviceId,uint32_t,X_INPUT_CAPABILITIES*) override { return 0; }
  X_RESULT GetDeviceKeystroke(DeviceId,uint32_t,X_INPUT_KEYSTROKE*) override { return X_ERROR_EMPTY; }
};
int main() {
  try {
    rex::InitLogging(nullptr,spdlog::level::warn);
    InitializeOptions();
    X_INPUT_GAMEPAD raw{}; raw.buttons=0xF7FF; raw.left_trigger=91; raw.right_trigger=205;
    raw.thumb_lx=-32768; raw.thumb_ly=32767; raw.thumb_rx=-1; raw.thumb_ry=219;
    const auto identity=Transform(raw,{});
    Check(std::memcmp(&raw,&identity,sizeof(raw))==0,"Default controls preserve input exactly");
    ControllerOptions options;
    options.mapping[10]=11; options.mapping[11]=10; options.mapping[14]=15; options.mapping[15]=14;
    raw.buttons=0x1000;
    auto transformed=Transform(raw,options);
    Check(uint16_t(transformed.buttons)==0x2000,"Swap face buttons");
    Check(transformed.left_trigger==205 && transformed.right_trigger==91,"Swapping triggers preserves analog strength");
    options.mapping[10]=-1; options.mapping[11]=14; raw.left_trigger=127;
    Check(uint16_t(Transform(raw,options).buttons)==0,"Unassigned button and trigger threshold");
    raw.left_trigger=128;
    Check(uint16_t(Transform(raw,options).buttons)==0x2000,"Trigger activates remapped button above threshold");
    int16_t x=2000,y=-2000; Deadzone(x,y,15);
    Check(x==0 && y==0,"Radial deadzone suppresses diagonal drift");
    x=20000;y=20000; Deadzone(x,y,15);
    Check(x==y && x>0 && x<20000,"Radial rescaling preserves direction");
    x=-32768;y=0; Deadzone(x,y,50);
    Check(x==-32768 && y==0,"Full stick throw preserved");
    options={}; options.invert_right_y=true; raw.thumb_ry=-32768;
    Check(int16_t(Transform(raw,options).thumb_ry)==32767,"Inversion does not wrap signed minimum");
    X_INPUT_VIBRATION motors{}; motors.left_motor_speed=65535; motors.right_motor_speed=10000;
    auto scaled=ScaleVibration(motors,25);
    Check(uint16_t(scaled.left_motor_speed)==16383 && uint16_t(scaled.right_motor_speed)==2500,"Rumble gain affects both motors");
    Check(uint16_t(ScaleVibration(motors,0).left_motor_speed)==0,"Zero disables vibration");
    auto mapping=options.mapping;
    Check(!ParseMapping("0,1,99",mapping) && mapping==options.mapping,"Malformed mappings preserve defaults");
    Check(ParseMapping(kDefaultMapping,mapping) && MappingString(mapping)==kDefaultMapping,"Mapping serialization round-trip");

    auto backend=std::make_unique<FakePad>(); auto* pad=backend.get();
    ControllerDriver driver(std::move(backend));
    bool active=true; driver.set_is_active_callback([&] { return active; });
    const auto id=static_cast<DeviceId>(1); X_INPUT_STATE state{};
    pad->state.gamepad.buttons=0x1000;
    driver.GetDeviceState(id,&state); const uint32_t packet=state.packet_number;
    active=false; driver.GetDeviceState(id,&state);
    Check(uint16_t(state.gamepad.buttons)==0 && uint32_t(state.packet_number)>packet,"Blocking guest input updates packet number");
    Check(uint16_t(driver.PollRaw().buttons)==0x1000,"Menu sees raw input while game is blocked");
    driver.SetDeviceVibration(id,&motors);
    Check(uint16_t(pad->vibration.left_motor_speed)==0,"Overlay suppresses rumble");
    active=true; driver.SuppressUntilReleased(); driver.GetDeviceState(id,&state);
    Check(uint16_t(state.gamepad.buttons)==0,"Closing overlay cannot replay held A into game");
    pad->state.gamepad.buttons=0; driver.GetDeviceState(id,&state);
    pad->state.gamepad.buttons=0x1000; driver.GetDeviceState(id,&state);
    Check(uint16_t(state.gamepad.buttons)==0x1000,"Fresh press works after release");
    pad->state.gamepad.buttons=0x30; driver.GetDeviceState(id,&state);
    Check(uint16_t(state.gamepad.buttons)==0,"Menu chord never reaches gameplay");
    pad->connected=false;
    Check(driver.GetDeviceState(id,&state)==X_ERROR_DEVICE_NOT_CONNECTED,"Disconnection propagates");
    Check(rex::cvar::SetFlagByName("iu_master_volume","25"),"Set volume");
    Check(std::abs(rex::audio::GetOutputGain()-0.25f)<0.001f,"Volume drives actual audio gain");
    Check(!rex::cvar::SetFlagByName("iu_master_volume","101"),"Volume cannot exceed unity gain");
    std::cout<<"PASS controller mapping, deadzones, vibration, menu isolation and audio gain\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
