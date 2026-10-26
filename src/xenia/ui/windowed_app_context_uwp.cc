/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/ui/windowed_app_context_uwp.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Core.h>

#include "xenia/ui/window_uwp.h"

namespace xe {
namespace ui {

UWPWindowedAppContext::UWPWindowedAppContext(IUnknown* core_window,
                                             uint32_t dpi)
    : core_window_(core_window), dpi_(dpi) {
  core_window_->AddRef();
  wake_event_ = CreateEventExW(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
}

UWPWindowedAppContext::~UWPWindowedAppContext() {
  CloseHandle(wake_event_);
  core_window_->Release();
}

void UWPWindowedAppContext::RequestPaint() {
  paint_requested_.store(true, std::memory_order_relaxed);
  SetEvent(wake_event_);
}

void UWPWindowedAppContext::RunMainLoop() {
  winrt::Windows::UI::Core::CoreWindow core_window{nullptr};
  winrt::copy_from_abi(core_window, core_window_);
  winrt::Windows::UI::Core::CoreDispatcher dispatcher =
      core_window.Dispatcher();
  while (!HasQuitFromUIThread()) {
    dispatcher.ProcessEvents(
        winrt::Windows::UI::Core::CoreProcessEventsOption::ProcessAllIfPresent);
    ExecutePendingFunctionsFromUIThread();
    if (HasQuitFromUIThread()) {
      break;
    }
    if (paint_requested_.exchange(false, std::memory_order_relaxed) &&
        window_) {
      window_->PaintFromMainLoop();
    }
    // CoreWindow events can't wake this wait, so keep the timeout short to
    // stay responsive to input.
    WaitForSingleObjectEx(wake_event_, 1, FALSE);
  }
}

void UWPWindowedAppContext::NotifyUILoopOfPendingFunctions() {
  SetEvent(wake_event_);
}

void UWPWindowedAppContext::PlatformQuitFromUIThread() {
  // HasQuitFromUIThread is already true, just wake up the loop to exit it.
  SetEvent(wake_event_);
}

}  // namespace ui
}  // namespace xe
