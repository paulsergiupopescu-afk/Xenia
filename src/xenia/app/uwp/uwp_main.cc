/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/platform_win.h"

#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.UI.Core.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

#include "xenia/app/uwp/uwp_emulator_app.h"
#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/base/main_win.h"
#include "xenia/base/platform_uwp.h"
#include "xenia/ui/windowed_app_context_uwp.h"

DECLARE_path(log_file);

namespace {
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::UI::Core;

// Extracts the game path from a protocol activation. Frontends for the Xbox
// such as RetroPass launch emulators with a "cmd" query parameter containing a
// command line - accept that, with or without the executable name first, and a
// plain "path" parameter.
std::filesystem::path GetProtocolLaunchPath(
    const ProtocolActivatedEventArgs& args) {
  auto query = args.Uri().QueryParsed();
  for (uint32_t i = 0; i < query.Size(); ++i) {
    auto entry = query.GetAt(i);
    std::wstring value(entry.Value());
    if (entry.Name() == L"path") {
      return value;
    }
    if (entry.Name() != L"cmd") {
      continue;
    }
    // Drop an executable name in front of the path.
    size_t exe = value.find(L".exe");
    if (exe != std::wstring::npos && exe + 4 <= value.size()) {
      value.erase(0, exe + 4);
    }
    std::wstring path;
    for (wchar_t c : value) {
      if (c != L'"') {
        path.push_back(c);
      }
    }
    size_t first = path.find_first_not_of(L' ');
    size_t last = path.find_last_not_of(L' ');
    if (first == std::wstring::npos) {
      return {};
    }
    return path.substr(first, last - first + 1);
  }
  return {};
}

struct App : winrt::implements<App, IFrameworkViewSource, IFrameworkView> {
  IFrameworkView CreateView() { return *this; }

  void Initialize(const CoreApplicationView& view) {
    view.Activated({this, &App::OnActivated});
  }

  void SetWindow(const CoreWindow&) {}
  void Load(const winrt::hstring&) {}
  void Uninitialize() {}

  void OnActivated(const CoreApplicationView&,
                   const IActivatedEventArgs& args) {
    if (args.Kind() == ActivationKind::Protocol) {
      launch_path_ =
          GetProtocolLaunchPath(args.as<ProtocolActivatedEventArgs>());
    }
    CoreWindow::GetForCurrentThread().Activate();
    activated_ = true;
  }

  void Run() {
    CoreWindow core_window = CoreWindow::GetForCurrentThread();
    // Get the activation (and a launch path in it) before starting.
    while (!activated_) {
      core_window.Dispatcher().ProcessEvents(
          CoreProcessEventsOption::ProcessAllIfPresent);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // B on the controller requests back navigation, which suspends the app
    // when unhandled.
    SystemNavigationManager::GetForCurrentView().BackRequested(
        [](const winrt::Windows::Foundation::IInspectable&,
           const BackRequestedEventArgs& e) { e.Handled(true); });

    uint32_t dpi = uint32_t(winrt::Windows::Graphics::Display::
                                DisplayInformation::GetForCurrentView()
                                    .LogicalDpi());

    // The package folder, the default log location, is read-only.
    cvars::log_file = xe::uwp::GetLocalStateFolder() / "xenia.log";
    xe::InitializeWin32App("xenia");
    if (!launch_path_.empty()) {
      XELOGI("Protocol launch path: {}", launch_path_);
    }

    {
      xe::ui::UWPWindowedAppContext app_context(
          static_cast<IUnknown*>(winrt::get_abi(core_window)), dpi ? dpi : 96);
      auto app = std::make_unique<xe::app::uwp::UWPEmulatorApp>(app_context);
      app->SetLaunchPath(launch_path_);
      if (app->OnInitialize()) {
        app_context.RunMainLoop();
      } else {
        XELOGE("Failed to initialize the app");
      }
      app->InvokeOnDestroy();
    }

    xe::ShutdownWin32App();
  }

 private:
  bool activated_ = false;
  std::filesystem::path launch_path_;
};
}  // namespace

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  winrt::init_apartment();
  CoreApplication::Run(winrt::make<App>());
  winrt::uninit_apartment();
  return 0;
}
