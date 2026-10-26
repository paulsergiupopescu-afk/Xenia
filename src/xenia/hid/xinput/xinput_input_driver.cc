/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/xinput/xinput_input_driver.h"

// Must be included before xinput.h to avoid windows.h conflicts.
#include "xenia/base/platform_win.h"

#include <xinput.h>  // NOLINT(build/include_order)

#include <chrono>

#include "xenia/base/logging.h"
#include "xenia/base/threading.h"
#include "xenia/hid/hid_flags.h"

namespace xe {
namespace hid {
namespace xinput {

XInputInputDriver::XInputInputDriver(xe::ui::Window* window,
                                     size_t window_z_order)
    : InputDriver(window, window_z_order) {}

XInputInputDriver::~XInputInputDriver() {
  hotplug_thread_quit_.store(true, std::memory_order_relaxed);
  if (hotplug_thread_.joinable()) {
    hotplug_thread_.join();
  }
  if (module_) {
    FreeLibrary(static_cast<HMODULE>(module_));
  }
}

X_STATUS XInputInputDriver::Setup() {
  // xinputuap.dll is the app container flavor, xinput1_4.dll is the desktop
  // one, with the guide button exposed through XInputGetStateEx.
  HMODULE module = LoadLibraryW(L"xinputuap.dll");
  if (!module) {
    module = LoadLibraryW(L"xinput1_4.dll");
  }
  if (!module) {
    XELOGE("XInput: Failed to load xinputuap.dll or xinput1_4.dll");
    return X_STATUS_DLL_NOT_FOUND;
  }

  auto get_capabilities = GetProcAddress(module, "XInputGetCapabilities");
  auto get_state = GetProcAddress(module, "XInputGetState");
  auto set_state = GetProcAddress(module, "XInputSetState");
  if (!get_capabilities || !get_state || !set_state) {
    XELOGE("XInput: Required functions are missing");
    FreeLibrary(module);
    return X_STATUS_PROCEDURE_NOT_FOUND;
  }
  module_ = module;
  get_capabilities_ = reinterpret_cast<void*>(get_capabilities);
  get_state_ = reinterpret_cast<void*>(get_state);
  set_state_ = reinterpret_cast<void*>(set_state);
  // Optional. XInputGetStateEx is exported only by ordinal 100.
  get_state_ex_ = reinterpret_cast<void*>(
      GetProcAddress(module, reinterpret_cast<LPCSTR>(100)));
  get_keystroke_ =
      reinterpret_cast<void*>(GetProcAddress(module, "XInputGetKeystroke"));

  connected_mask_.store(QueryConnectedMask(), std::memory_order_relaxed);
  hotplug_thread_ = std::thread(&XInputInputDriver::HotplugThread, this);
  return X_STATUS_SUCCESS;
}

uint32_t XInputInputDriver::QueryConnectedMask() const {
  auto get_capabilities =
      reinterpret_cast<decltype(&XInputGetCapabilities)>(get_capabilities_);
  uint32_t mask = 0;
  for (uint32_t i = 0; i < kMaxControllers; ++i) {
    XINPUT_CAPABILITIES caps;
    if (get_capabilities(i, 0, &caps) == ERROR_SUCCESS) {
      mask |= uint32_t(1) << i;
    }
  }
  return mask;
}

void XInputInputDriver::HotplugThread() {
  xe::threading::set_name("XInput Hotplug");
  while (!hotplug_thread_quit_.load(std::memory_order_relaxed)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    uint32_t mask = QueryConnectedMask();
    if (connected_mask_.exchange(mask, std::memory_order_relaxed) != mask) {
      NotifyDevicesChanged();
    }
  }
}

std::vector<InputDeviceInfo> XInputInputDriver::EnumerateDevices() {
  std::vector<InputDeviceInfo> devices;
  uint32_t mask = connected_mask_.load(std::memory_order_relaxed);
  for (uint32_t i = 0; i < kMaxControllers; ++i) {
    if (!(mask & (uint32_t(1) << i))) {
      continue;
    }
    InputDeviceInfo& info = devices.emplace_back();
    info.driver_slot = uint8_t(i);
    info.stable_id = "xinput:" + std::to_string(i);
    info.display_name = "Controller " + std::to_string(i + 1);
    info.preferred_slot = int8_t(i);
  }
  return devices;
}

X_RESULT XInputInputDriver::GetCapabilities(uint32_t user_index, uint32_t flags,
                                            X_INPUT_CAPABILITIES* out_caps) {
  if (user_index >= kMaxControllers) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  auto get_capabilities =
      reinterpret_cast<decltype(&XInputGetCapabilities)>(get_capabilities_);
  XINPUT_CAPABILITIES native_caps;
  DWORD result = get_capabilities(user_index, flags, &native_caps);
  if (result != ERROR_SUCCESS) {
    return result;
  }
  out_caps->type = native_caps.Type;
  out_caps->sub_type = native_caps.SubType;
  out_caps->flags = native_caps.Flags;
  out_caps->gamepad.buttons = native_caps.Gamepad.wButtons;
  out_caps->gamepad.left_trigger = native_caps.Gamepad.bLeftTrigger;
  out_caps->gamepad.right_trigger = native_caps.Gamepad.bRightTrigger;
  out_caps->gamepad.thumb_lx = native_caps.Gamepad.sThumbLX;
  out_caps->gamepad.thumb_ly = native_caps.Gamepad.sThumbLY;
  out_caps->gamepad.thumb_rx = native_caps.Gamepad.sThumbRX;
  out_caps->gamepad.thumb_ry = native_caps.Gamepad.sThumbRY;
  out_caps->vibration.left_motor_speed = native_caps.Vibration.wLeftMotorSpeed;
  out_caps->vibration.right_motor_speed =
      native_caps.Vibration.wRightMotorSpeed;
  return X_ERROR_SUCCESS;
}

X_RESULT XInputInputDriver::GetState(uint32_t user_index,
                                     X_INPUT_STATE* out_state) {
  if (user_index >= kMaxControllers) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  // XInputGetStateEx writes a larger structure than XINPUT_STATE.
  struct {
    XINPUT_STATE state;
    uint32_t reserved;
  } native_state;
  auto get_state = reinterpret_cast<decltype(&XInputGetState)>(
      (cvars::guide_button && get_state_ex_) ? get_state_ex_ : get_state_);
  DWORD result = get_state(user_index, &native_state.state);
  if (result != ERROR_SUCCESS) {
    return result;
  }
  out_state->packet_number = native_state.state.dwPacketNumber;
  out_state->gamepad.buttons = native_state.state.Gamepad.wButtons;
  out_state->gamepad.left_trigger = native_state.state.Gamepad.bLeftTrigger;
  out_state->gamepad.right_trigger = native_state.state.Gamepad.bRightTrigger;
  out_state->gamepad.thumb_lx = native_state.state.Gamepad.sThumbLX;
  out_state->gamepad.thumb_ly = native_state.state.Gamepad.sThumbLY;
  out_state->gamepad.thumb_rx = native_state.state.Gamepad.sThumbRX;
  out_state->gamepad.thumb_ry = native_state.state.Gamepad.sThumbRY;
  return X_ERROR_SUCCESS;
}

X_RESULT XInputInputDriver::SetState(uint32_t user_index,
                                     X_INPUT_VIBRATION* vibration) {
  if (user_index >= kMaxControllers) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  auto set_state = reinterpret_cast<decltype(&XInputSetState)>(set_state_);
  XINPUT_VIBRATION native_vibration;
  native_vibration.wLeftMotorSpeed = vibration->left_motor_speed;
  native_vibration.wRightMotorSpeed = vibration->right_motor_speed;
  return set_state(user_index, &native_vibration);
}

X_RESULT XInputInputDriver::GetKeystroke(uint32_t user_index, uint32_t flags,
                                         X_INPUT_KEYSTROKE* out_keystroke) {
  if (!get_keystroke_) {
    return X_ERROR_EMPTY;
  }
  // XInputGetKeystroke returns ERROR_SUCCESS for a disconnected device with
  // garbage output, so check the connection first (not possible for "any
  // user", 0xFF, which doesn't have that problem).
  if (user_index != 0xFF) {
    if (user_index >= kMaxControllers) {
      return X_ERROR_DEVICE_NOT_CONNECTED;
    }
    auto get_capabilities =
        reinterpret_cast<decltype(&XInputGetCapabilities)>(get_capabilities_);
    XINPUT_CAPABILITIES caps;
    DWORD result = get_capabilities(user_index, 0, &caps);
    if (result != ERROR_SUCCESS) {
      return result;
    }
  }
  auto get_keystroke =
      reinterpret_cast<decltype(&XInputGetKeystroke)>(get_keystroke_);
  XINPUT_KEYSTROKE native_keystroke;
  DWORD result = get_keystroke(user_index, 0, &native_keystroke);
  if (result != ERROR_SUCCESS) {
    return result;
  }
  out_keystroke->virtual_key = native_keystroke.VirtualKey;
  out_keystroke->unicode = native_keystroke.Unicode;
  out_keystroke->flags = native_keystroke.Flags;
  out_keystroke->user_index = native_keystroke.UserIndex;
  out_keystroke->hid_code = native_keystroke.HidCode;
  return X_ERROR_SUCCESS;
}

}  // namespace xinput
}  // namespace hid
}  // namespace xe
