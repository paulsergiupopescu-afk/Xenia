/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/platform_uwp.h"

#include "xenia/base/platform_win.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.Profile.h>

namespace xe {
namespace uwp {

std::filesystem::path GetLocalStateFolder() {
  return std::filesystem::path(std::wstring_view(
      winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path()));
}

std::filesystem::path GetLocalCacheFolder() {
  return std::filesystem::path(
      std::wstring_view(winrt::Windows::Storage::ApplicationData::Current()
                            .LocalCacheFolder()
                            .Path()));
}

bool IsXbox() {
  static const bool is_xbox =
      winrt::Windows::System::Profile::AnalyticsInfo::VersionInfo()
          .DeviceFamily() == L"Windows.Xbox";
  return is_xbox;
}

}  // namespace uwp
}  // namespace xe
