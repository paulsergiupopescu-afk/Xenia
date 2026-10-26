/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <map>
#include "xenia/base/platform_win.h"
#include "xenia/base/string.h"
#include "xenia/base/system.h"

#if XE_PLATFORM_WINUWP
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

#include "xenia/base/filesystem.h"
#include "xenia/base/logging.h"
#endif  // XE_PLATFORM_WINUWP

namespace xe {

#if XE_PLATFORM_WINUWP
// ShellExecute and MessageBox aren't available to app containers.

void LaunchWebBrowser(const std::string_view url) {
  try {
    winrt::Windows::System::Launcher::LaunchUriAsync(
        winrt::Windows::Foundation::Uri(winrt::to_hstring(url)));
  } catch (const winrt::hresult_error& e) {
    XELOGE("Failed to open {}: {}", url, winrt::to_string(e.message()));
  }
}

void LaunchFileExplorer(const std::filesystem::path& url) {
  try {
    winrt::Windows::System::Launcher::LaunchFolderPathAsync(url.wstring());
  } catch (const winrt::hresult_error& e) {
    XELOGE("Failed to open {}: {}", xe::path_to_utf8(url),
           winrt::to_string(e.message()));
  }
}

void ShowSimpleMessageBox(SimpleMessageBoxType type,
                          const std::string_view message) {
  // A MessageDialog is asynchronous and needs the UI thread, while the callers
  // (fatal errors, invalid command lines) exit right after this returns, so
  // the message only goes to the log.
  XELOGE("{}", message);
}
#else
void LaunchWebBrowser(const std::string_view url) {
  auto wide_url = xe::to_utf16(url);
  ShellExecuteW(nullptr, L"open", reinterpret_cast<LPCWSTR>(wide_url.c_str()),
                nullptr, nullptr, SW_SHOWNORMAL);
}

void LaunchFileExplorer(const std::filesystem::path& url) {
  ShellExecuteW(nullptr, L"explore", url.c_str(), nullptr, nullptr,
                SW_SHOWNORMAL);
}

void ShowSimpleMessageBox(SimpleMessageBoxType type,
                          const std::string_view message) {
  const wchar_t* title;
  std::u16string wide_message = xe::to_utf16(message);
  DWORD type_flags = MB_OK | MB_APPLMODAL | MB_SETFOREGROUND;
  switch (type) {
    default:
    case SimpleMessageBoxType::Help:
      title = L"Xenia Help";
      type_flags |= MB_ICONINFORMATION;
      break;
    case SimpleMessageBoxType::Warning:
      title = L"Xenia Warning";
      type_flags |= MB_ICONWARNING;
      break;
    case SimpleMessageBoxType::Error:
      title = L"Xenia Error";
      type_flags |= MB_ICONERROR;
      break;
  }
  MessageBoxW(nullptr, reinterpret_cast<LPCWSTR>(wide_message.c_str()), title,
              type_flags);
}
#endif  // XE_PLATFORM_WINUWP

static std::map<const uint32_t, DWORD> xeniaToWindowsPriorityClassMapping = {
    {0, NORMAL_PRIORITY_CLASS},
    {1, ABOVE_NORMAL_PRIORITY_CLASS},
    {2, HIGH_PRIORITY_CLASS},
    {3, REALTIME_PRIORITY_CLASS}};

bool SetProcessPriorityClass(const uint32_t priority_class) {
  if (!xeniaToWindowsPriorityClassMapping.count(priority_class)) {
    return false;
  }

  return SetPriorityClass(GetCurrentProcess(),
                          xeniaToWindowsPriorityClassMapping[priority_class]);
}

bool IsUseNexusForGameBarEnabled() {
  constexpr LPCWSTR reg_path = L"SOFTWARE\\Microsoft\\GameBar";
  constexpr LPCWSTR key = L"UseNexusForGameBarEnabled";

  DWORD value = 0;
  DWORD dataSize = sizeof(value);

  RegGetValue(HKEY_CURRENT_USER, reg_path, key, RRF_RT_DWORD, nullptr, &value,
              &dataSize);

  return static_cast<bool>(value);
}

}  // namespace xe
