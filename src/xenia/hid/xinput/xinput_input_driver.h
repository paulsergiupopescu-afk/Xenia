/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_XINPUT_XINPUT_INPUT_DRIVER_H_
#define XENIA_HID_XINPUT_XINPUT_INPUT_DRIVER_H_

#include <atomic>
#include <cstdint>
#include <thread>

#include "xenia/hid/input_driver.h"

namespace xe {
namespace hid {
namespace xinput {

// XInput controllers. The only controller driver of the UWP app: SDL can't
// enumerate devices from an app container, while XInput (xinputuap.dll) is the
// gamepad API UWP apps get on the Xbox.
class XInputInputDriver final : public InputDriver {
 public:
  explicit XInputInputDriver(xe::ui::Window* window, size_t window_z_order);
  ~XInputInputDriver() override;

  X_STATUS Setup() override;

  X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags,
                           X_INPUT_CAPABILITIES* out_caps) override;
  X_RESULT GetState(uint32_t user_index, X_INPUT_STATE* out_state) override;
  X_RESULT SetState(uint32_t user_index, X_INPUT_VIBRATION* vibration) override;
  X_RESULT GetKeystroke(uint32_t user_index, uint32_t flags,
                        X_INPUT_KEYSTROKE* out_keystroke) override;

  InputType GetInputType() const override { return InputType::Controller; }

  std::vector<InputDeviceInfo> EnumerateDevices() override;

 private:
  static constexpr uint32_t kMaxControllers = 4;

  // Bit i set if XInput user i is connected.
  uint32_t QueryConnectedMask() const;
  void HotplugThread();

  void* module_ = nullptr;
  void* get_capabilities_ = nullptr;
  void* get_state_ = nullptr;
  void* get_state_ex_ = nullptr;
  void* get_keystroke_ = nullptr;
  void* set_state_ = nullptr;

  // XInput only reports connection changes when polled, so a background thread
  // watches for them and lets the input system rebind slots.
  std::atomic<bool> hotplug_thread_quit_ = false;
  std::atomic<uint32_t> connected_mask_ = 0;
  std::thread hotplug_thread_;
};

}  // namespace xinput
}  // namespace hid
}  // namespace xe

#endif  // XENIA_HID_XINPUT_XINPUT_INPUT_DRIVER_H_
