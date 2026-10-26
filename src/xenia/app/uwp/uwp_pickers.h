/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_APP_UWP_UWP_PICKERS_H_
#define XENIA_APP_UWP_UWP_PICKERS_H_

#include <filesystem>
#include <functional>

#include "xenia/ui/windowed_app_context.h"

namespace xe {
namespace app {
namespace uwp {

using PickCallback = std::function<void(const std::filesystem::path& path)>;

// Show the system file / folder picker. Must be called from the UI thread. The
// callback is invoked from the UI thread, with an empty path if cancelled.
void PickFile(ui::WindowedAppContext& app_context, PickCallback callback);
void PickFolder(ui::WindowedAppContext& app_context, PickCallback callback);

}  // namespace uwp
}  // namespace app
}  // namespace xe

#endif  // XENIA_APP_UWP_UWP_PICKERS_H_
