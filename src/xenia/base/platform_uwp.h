/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_BASE_PLATFORM_UWP_H_
#define XENIA_BASE_PLATFORM_UWP_H_

#include <filesystem>

namespace xe {
namespace uwp {

// The app's writable storage (ApplicationData::LocalFolder, "LocalState"). The
// package install folder is read-only, so config, content, logs and caches
// all live under here.
std::filesystem::path GetLocalStateFolder();

// ApplicationData::LocalCacheFolder, not backed up, for regenerable files.
std::filesystem::path GetLocalCacheFolder();

// Whether the app runs on an Xbox console rather than on a Windows PC.
bool IsXbox();

}  // namespace uwp
}  // namespace xe

#endif  // XENIA_BASE_PLATFORM_UWP_H_
