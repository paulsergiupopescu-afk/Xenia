/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/app/uwp/uwp_pickers.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.h>

#include "xenia/base/logging.h"

namespace xe {
namespace app {
namespace uwp {

namespace {
namespace wsp = winrt::Windows::Storage::Pickers;

// The coroutine may resume on a thread pool thread, so the result is posted
// back to the UI thread.
winrt::fire_and_forget PickFileAsync(ui::WindowedAppContext& app_context,
                                     PickCallback callback) {
  std::filesystem::path path;
  try {
    wsp::FileOpenPicker picker;
    picker.ViewMode(wsp::PickerViewMode::List);
    picker.SuggestedStartLocation(wsp::PickerLocationId::ComputerFolder);
    picker.FileTypeFilter().Append(L"*");
    winrt::Windows::Storage::StorageFile file =
        co_await picker.PickSingleFileAsync();
    if (file) {
      path = std::wstring_view(file.Path());
    }
  } catch (const winrt::hresult_error& e) {
    XELOGE("UWP: File picker failed: {}", winrt::to_string(e.message()));
  }
  app_context.CallInUIThread(
      [callback = std::move(callback), path]() { callback(path); });
}

winrt::fire_and_forget PickFolderAsync(ui::WindowedAppContext& app_context,
                                       PickCallback callback) {
  std::filesystem::path path;
  try {
    wsp::FolderPicker picker;
    picker.ViewMode(wsp::PickerViewMode::List);
    picker.SuggestedStartLocation(wsp::PickerLocationId::ComputerFolder);
    picker.FileTypeFilter().Append(L"*");
    winrt::Windows::Storage::StorageFolder folder =
        co_await picker.PickSingleFolderAsync();
    if (folder) {
      path = std::wstring_view(folder.Path());
    }
  } catch (const winrt::hresult_error& e) {
    XELOGE("UWP: Folder picker failed: {}", winrt::to_string(e.message()));
  }
  app_context.CallInUIThread(
      [callback = std::move(callback), path]() { callback(path); });
}
}  // namespace

void PickFile(ui::WindowedAppContext& app_context, PickCallback callback) {
  PickFileAsync(app_context, std::move(callback));
}

void PickFolder(ui::WindowedAppContext& app_context, PickCallback callback) {
  PickFolderAsync(app_context, std::move(callback));
}

}  // namespace uwp
}  // namespace app
}  // namespace xe
