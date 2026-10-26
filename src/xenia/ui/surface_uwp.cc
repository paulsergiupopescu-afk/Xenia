/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/ui/surface_win.h"

#include <winrt/Windows.Graphics.Display.h>

#include "xenia/ui/window_uwp.h"

namespace xe {
namespace ui {

UWPCoreWindowSurface::UWPCoreWindowSurface(IUnknown* core_window)
    : core_window_(core_window) {
  core_window_->AddRef();
}

UWPCoreWindowSurface::~UWPCoreWindowSurface() { core_window_->Release(); }

bool UWPCoreWindowSurface::GetSizeImpl(uint32_t& width_out,
                                       uint32_t& height_out) const {
  uint32_t dpi = uint32_t(winrt::Windows::Graphics::Display::
                              DisplayInformation::GetForCurrentView()
                                  .LogicalDpi());
  return GetUWPCoreWindowPhysicalSize(core_window_, dpi ? dpi : 96, width_out,
                                      height_out);
}

}  // namespace ui
}  // namespace xe
