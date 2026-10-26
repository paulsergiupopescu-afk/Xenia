/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/ui/window_uwp.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Display.Core.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.h>

#include "xenia/base/assert.h"
#include "xenia/base/platform_uwp.h"
#include "xenia/ui/surface_win.h"
#include "xenia/ui/ui_event.h"
#include "xenia/ui/virtual_key.h"
#include "xenia/ui/windowed_app_context_uwp.h"

namespace xe {
namespace ui {

namespace {
namespace wuc = winrt::Windows::UI::Core;

wuc::CoreWindow CoreWindowFromAbi(IUnknown* core_window) {
  wuc::CoreWindow window{nullptr};
  winrt::copy_from_abi(window, core_window);
  return window;
}

bool IsKeyDown(const wuc::CoreWindow& window,
               winrt::Windows::System::VirtualKey key) {
  return (window.GetKeyState(key) & wuc::CoreVirtualKeyStates::Down) ==
         wuc::CoreVirtualKeyStates::Down;
}
}  // namespace

bool GetUWPCoreWindowPhysicalSize(IUnknown* core_window, uint32_t dpi,
                                  uint32_t& width_out, uint32_t& height_out) {
  if (xe::uwp::IsXbox()) {
    auto hdmi = winrt::Windows::Graphics::Display::Core::
        HdmiDisplayInformation::GetForCurrentView();
    if (hdmi) {
      auto mode = hdmi.GetCurrentDisplayMode();
      width_out = mode.ResolutionWidthInRawPixels();
      height_out = mode.ResolutionHeightInRawPixels();
      return width_out && height_out;
    }
  }
  winrt::Windows::Foundation::Rect bounds =
      CoreWindowFromAbi(core_window).Bounds();
  width_out = uint32_t(bounds.Width * float(dpi) / 96.0f + 0.5f);
  height_out = uint32_t(bounds.Height * float(dpi) / 96.0f + 0.5f);
  return width_out && height_out;
}

struct UWPWindow::EventHandlers {
  wuc::CoreWindow window{nullptr};
  winrt::event_token size_changed;
  winrt::event_token activated;
  winrt::event_token key_down;
  winrt::event_token key_up;
  winrt::event_token character_received;
  winrt::event_token pointer_pressed;
  winrt::event_token pointer_moved;
  winrt::event_token pointer_released;
  winrt::event_token pointer_wheel_changed;
};

UWPWindow::UWPWindow(WindowedAppContext& app_context,
                     const std::string_view title,
                     uint32_t desired_logical_width,
                     uint32_t desired_logical_height)
    : Window(app_context, title, desired_logical_width,
             desired_logical_height) {}

UWPWindow::~UWPWindow() {
  RevokeEventHandlers();
  auto& uwp_app_context = static_cast<UWPWindowedAppContext&>(app_context());
  if (uwp_app_context.window() == this) {
    uwp_app_context.SetWindow(nullptr);
  }
}

uint32_t UWPWindow::GetLatestDpiImpl() const {
  return static_cast<const UWPWindowedAppContext&>(app_context()).dpi();
}

void UWPWindow::UpdateSize() {
  auto& uwp_app_context = static_cast<UWPWindowedAppContext&>(app_context());
  uint32_t width, height;
  if (!GetUWPCoreWindowPhysicalSize(uwp_app_context.core_window(),
                                    uwp_app_context.dpi(), width, height)) {
    return;
  }
  WindowDestructionReceiver destruction_receiver(this);
  OnActualSizeUpdate(width, height, destruction_receiver);
}

bool UWPWindow::OpenImpl() {
  auto& uwp_app_context = static_cast<UWPWindowedAppContext&>(app_context());
  // There's only one CoreWindow.
  assert_null(uwp_app_context.window());
  if (uwp_app_context.window()) {
    return false;
  }
  uwp_app_context.SetWindow(this);

  // The CoreWindow is fullscreen-only.
  OnDesiredFullscreenUpdate(true);

  event_handlers_ = std::make_unique<EventHandlers>();
  EventHandlers& handlers = *event_handlers_;
  handlers.window = CoreWindowFromAbi(uwp_app_context.core_window());
  wuc::CoreWindow& core_window = handlers.window;

  handlers.size_changed = core_window.SizeChanged(
      [this](const wuc::CoreWindow&, const wuc::WindowSizeChangedEventArgs&) {
        UpdateSize();
      });
  handlers.activated = core_window.Activated(
      [this](const wuc::CoreWindow&, const wuc::WindowActivatedEventArgs& e) {
        WindowDestructionReceiver destruction_receiver(this);
        OnFocusUpdate(
            e.WindowActivationState() != wuc::CoreWindowActivationState::Deactivated,
            destruction_receiver);
      });

  auto make_key_event = [this](const wuc::CoreWindow& sender,
                               const wuc::KeyEventArgs& e) {
    using winrt::Windows::System::VirtualKey;
    return KeyEvent(this, xe::ui::VirtualKey(uint16_t(e.VirtualKey())),
                    int(e.KeyStatus().RepeatCount), e.KeyStatus().WasKeyDown,
                    IsKeyDown(sender, VirtualKey::Shift),
                    IsKeyDown(sender, VirtualKey::Control),
                    IsKeyDown(sender, VirtualKey::Menu),
                    IsKeyDown(sender, VirtualKey::LeftWindows) ||
                        IsKeyDown(sender, VirtualKey::RightWindows));
  };
  handlers.key_down = core_window.KeyDown(
      [this, make_key_event](const wuc::CoreWindow& sender,
                             const wuc::KeyEventArgs& e) {
        KeyEvent key_event = make_key_event(sender, e);
        WindowDestructionReceiver destruction_receiver(this);
        OnKeyDown(key_event, destruction_receiver);
        if (!destruction_receiver.IsWindowDestroyed()) {
          e.Handled(key_event.is_handled());
        }
      });
  handlers.key_up = core_window.KeyUp(
      [this, make_key_event](const wuc::CoreWindow& sender,
                             const wuc::KeyEventArgs& e) {
        KeyEvent key_event = make_key_event(sender, e);
        WindowDestructionReceiver destruction_receiver(this);
        OnKeyUp(key_event, destruction_receiver);
        if (!destruction_receiver.IsWindowDestroyed()) {
          e.Handled(key_event.is_handled());
        }
      });
  handlers.character_received = core_window.CharacterReceived(
      [this](const wuc::CoreWindow&, const wuc::CharacterReceivedEventArgs& e) {
        // VirtualKey carries the character, as with WM_CHAR.
        KeyEvent key_event(this, xe::ui::VirtualKey(uint16_t(e.KeyCode())), 1,
                           false, false, false, false, false);
        WindowDestructionReceiver destruction_receiver(this);
        OnKeyChar(key_event, destruction_receiver);
      });

  auto make_mouse_event = [this](const wuc::PointerEventArgs& e) {
    using winrt::Windows::UI::Input::PointerUpdateKind;
    winrt::Windows::UI::Input::PointerPoint point = e.CurrentPoint();
    MouseEvent::Button button = MouseEvent::Button::kNone;
    switch (point.Properties().PointerUpdateKind()) {
      case PointerUpdateKind::LeftButtonPressed:
      case PointerUpdateKind::LeftButtonReleased:
        button = MouseEvent::Button::kLeft;
        break;
      case PointerUpdateKind::RightButtonPressed:
      case PointerUpdateKind::RightButtonReleased:
        button = MouseEvent::Button::kRight;
        break;
      case PointerUpdateKind::MiddleButtonPressed:
      case PointerUpdateKind::MiddleButtonReleased:
        button = MouseEvent::Button::kMiddle;
        break;
      case PointerUpdateKind::XButton1Pressed:
      case PointerUpdateKind::XButton1Released:
        button = MouseEvent::Button::kX1;
        break;
      case PointerUpdateKind::XButton2Pressed:
      case PointerUpdateKind::XButton2Released:
        button = MouseEvent::Button::kX2;
        break;
      default:
        break;
    }
    uint32_t dpi = GetDpi();
    int32_t wheel = point.Properties().MouseWheelDelta();
    return MouseEvent(
        this, button,
        int32_t(point.Position().X * float(dpi) / 96.0f),
        int32_t(point.Position().Y * float(dpi) / 96.0f), 0,
        point.Properties().IsHorizontalMouseWheel() ? 0 : wheel);
  };
  handlers.pointer_pressed = core_window.PointerPressed(
      [this, make_mouse_event](const wuc::CoreWindow&,
                               const wuc::PointerEventArgs& e) {
        MouseEvent mouse_event = make_mouse_event(e);
        WindowDestructionReceiver destruction_receiver(this);
        OnMouseDown(mouse_event, destruction_receiver);
      });
  handlers.pointer_moved = core_window.PointerMoved(
      [this, make_mouse_event](const wuc::CoreWindow&,
                               const wuc::PointerEventArgs& e) {
        MouseEvent mouse_event = make_mouse_event(e);
        WindowDestructionReceiver destruction_receiver(this);
        OnMouseMove(mouse_event, destruction_receiver);
      });
  handlers.pointer_released = core_window.PointerReleased(
      [this, make_mouse_event](const wuc::CoreWindow&,
                               const wuc::PointerEventArgs& e) {
        MouseEvent mouse_event = make_mouse_event(e);
        WindowDestructionReceiver destruction_receiver(this);
        OnMouseUp(mouse_event, destruction_receiver);
      });
  handlers.pointer_wheel_changed = core_window.PointerWheelChanged(
      [this, make_mouse_event](const wuc::CoreWindow&,
                               const wuc::PointerEventArgs& e) {
        MouseEvent mouse_event = make_mouse_event(e);
        WindowDestructionReceiver destruction_receiver(this);
        OnMouseWheel(mouse_event, destruction_receiver);
      });

  UpdateSize();
  WindowDestructionReceiver destruction_receiver(this);
  OnFocusUpdate(true, destruction_receiver);
  return true;
}

void UWPWindow::RevokeEventHandlers() {
  if (!event_handlers_) {
    return;
  }
  EventHandlers& handlers = *event_handlers_;
  wuc::CoreWindow& core_window = handlers.window;
  core_window.SizeChanged(handlers.size_changed);
  core_window.Activated(handlers.activated);
  core_window.KeyDown(handlers.key_down);
  core_window.KeyUp(handlers.key_up);
  core_window.CharacterReceived(handlers.character_received);
  core_window.PointerPressed(handlers.pointer_pressed);
  core_window.PointerMoved(handlers.pointer_moved);
  core_window.PointerReleased(handlers.pointer_released);
  core_window.PointerWheelChanged(handlers.pointer_wheel_changed);
  event_handlers_.reset();
}

void UWPWindow::RequestCloseImpl() {
  // The CoreWindow itself stays, only detach from it, like the Android window
  // detaches from the activity. Quitting is up to the listeners.
  RevokeEventHandlers();
  WindowDestructionReceiver destruction_receiver(this);
  OnBeforeClose(destruction_receiver);
  if (destruction_receiver.IsWindowDestroyed()) {
    return;
  }
  OnAfterClose();
  auto& uwp_app_context = static_cast<UWPWindowedAppContext&>(app_context());
  if (uwp_app_context.window() == this) {
    uwp_app_context.SetWindow(nullptr);
  }
}

std::unique_ptr<Surface> UWPWindow::CreateSurfaceImpl(
    Surface::TypeFlags allowed_types) {
  if (allowed_types & Surface::kTypeFlag_UWPCoreWindow) {
    return std::make_unique<UWPCoreWindowSurface>(
        static_cast<UWPWindowedAppContext&>(app_context()).core_window());
  }
  return nullptr;
}

void UWPWindow::RequestPaintImpl() {
  static_cast<UWPWindowedAppContext&>(app_context()).RequestPaint();
}

std::unique_ptr<Window> Window::Create(WindowedAppContext& app_context,
                                       const std::string_view title,
                                       uint32_t desired_logical_width,
                                       uint32_t desired_logical_height) {
  return std::make_unique<UWPWindow>(app_context, title, desired_logical_width,
                                     desired_logical_height);
}

std::unique_ptr<MenuItem> MenuItem::Create(Type type, const std::string& text,
                                           const std::string& hotkey,
                                           std::function<void()> callback) {
  return std::make_unique<UWPMenuItem>(type, text, hotkey,
                                       std::move(callback));
}

std::unique_ptr<MenuItem> MenuItem::CreateCheck(
    const std::string& text, bool initially_checked,
    std::function<void(bool)> on_toggle) {
  return std::make_unique<UWPMenuItem>(Type::kCheck, text, initially_checked,
                                       std::move(on_toggle));
}

}  // namespace ui
}  // namespace xe
