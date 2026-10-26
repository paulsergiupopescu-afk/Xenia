/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_WINDOW_UWP_H_
#define XENIA_UI_WINDOW_UWP_H_

#include <cstdint>
#include <memory>
#include <string>

#include "xenia/base/platform_win.h"
#include "xenia/ui/menu_item.h"
#include "xenia/ui/window.h"

namespace xe {
namespace ui {

// Size of the CoreWindow in physical pixels. On the Xbox this is the current
// HDMI output mode, as the CoreWindow bounds are always 1080p there.
bool GetUWPCoreWindowPhysicalSize(IUnknown* core_window, uint32_t dpi,
                                  uint32_t& width_out, uint32_t& height_out);

// A proxy for the CoreWindow of the app's view, which is always fullscreen.
class UWPWindow : public Window {
 public:
  UWPWindow(WindowedAppContext& app_context, const std::string_view title,
            uint32_t desired_logical_width, uint32_t desired_logical_height);
  ~UWPWindow() override;

  void PaintFromMainLoop() { OnPaint(); }

 protected:
  uint32_t GetLatestDpiImpl() const override;

  bool OpenImpl() override;
  void RequestCloseImpl() override;

  std::unique_ptr<Surface> CreateSurfaceImpl(
      Surface::TypeFlags allowed_types) override;
  void RequestPaintImpl() override;

 private:
  struct EventHandlers;

  void UpdateSize();
  void RevokeEventHandlers();

  std::unique_ptr<EventHandlers> event_handlers_;
};

class UWPMenuItem final : public MenuItem {
 public:
  UWPMenuItem(Type type, const std::string& text, const std::string& hotkey,
              std::function<void()> callback)
      : MenuItem(type, text, hotkey, std::move(callback)) {}
  UWPMenuItem(Type type, const std::string& text, bool initially_checked,
              std::function<void(bool)> on_toggle)
      : MenuItem(type, text, initially_checked, std::move(on_toggle)) {}
};

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_WINDOW_UWP_H_
