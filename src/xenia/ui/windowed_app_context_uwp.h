/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_WINDOWED_APP_CONTEXT_UWP_H_
#define XENIA_UI_WINDOWED_APP_CONTEXT_UWP_H_

#include <atomic>
#include <cstdint>

#include "xenia/base/platform_win.h"
#include "xenia/ui/windowed_app_context.h"

namespace xe {
namespace ui {

class UWPWindow;

// The UI thread is the thread of the app view's CoreWindow. A UWP view has one
// CoreWindow, so there's at most one open xe::ui::Window presenting to it.
class UWPWindowedAppContext final : public WindowedAppContext {
 public:
  // Must be created on the CoreWindow's thread. core_window is the ABI
  // IUnknown of the CoreWindow, referenced for the lifetime of the context.
  UWPWindowedAppContext(IUnknown* core_window, uint32_t dpi);
  ~UWPWindowedAppContext() override;

  IUnknown* core_window() const { return core_window_; }
  uint32_t dpi() const { return dpi_; }

  UWPWindow* window() const { return window_; }
  void SetWindow(UWPWindow* window) { window_ = window; }

  // Called by the window instead of painting directly, so painting is
  // coalesced and done from the main loop.
  void RequestPaint();

  // Dispatches the CoreWindow events, the pending functions and paint requests
  // until QuitFromUIThread.
  void RunMainLoop();

 protected:
  void NotifyUILoopOfPendingFunctions() override;
  void PlatformQuitFromUIThread() override;

 private:
  IUnknown* core_window_;
  uint32_t dpi_;
  UWPWindow* window_ = nullptr;
  HANDLE wake_event_;
  std::atomic<bool> paint_requested_ = false;
};

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_WINDOWED_APP_CONTEXT_UWP_H_
