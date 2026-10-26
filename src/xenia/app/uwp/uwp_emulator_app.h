/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_APP_UWP_UWP_EMULATOR_APP_H_
#define XENIA_APP_UWP_UWP_EMULATOR_APP_H_

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "xenia/base/threading.h"
#include "xenia/emulator.h"
#include "xenia/ui/imgui_drawer.h"
#include "xenia/ui/immediate_drawer.h"
#include "xenia/ui/presenter.h"
#include "xenia/ui/window.h"
#include "xenia/ui/window_listener.h"
#include "xenia/ui/windowed_app.h"

namespace xe {
namespace app {
namespace uwp {

class GameListDialog;

// The emulator frontend of the UWP app. The desktop frontend is built on
// wxWidgets, which doesn't exist for UWP, so this is a small ImGui-only one:
// a game list, then the game.
class UWPEmulatorApp final : public ui::WindowedApp {
 public:
  struct GameEntry {
    std::string name;
    std::filesystem::path path;
  };

  enum : size_t {
    kZOrderHidInput,
    kZOrderImGui,
  };

  explicit UWPEmulatorApp(ui::WindowedAppContext& app_context);
  ~UWPEmulatorApp() override;

  static std::unique_ptr<ui::WindowedApp> Create(
      ui::WindowedAppContext& app_context) {
    return std::make_unique<UWPEmulatorApp>(app_context);
  }

  // A path to launch right away instead of showing the game list, from a
  // protocol activation (launchers like RetroArch use this). Set before
  // OnInitialize.
  void SetLaunchPath(const std::filesystem::path& path) {
    launch_path_ = path;
  }

  bool OnInitialize() override;

  // UI thread only.
  const std::vector<GameEntry>& games() const { return games_; }
  const std::vector<std::filesystem::path>& game_folders() const {
    return game_folders_;
  }
  void RefreshGames();
  void AddGameFolder(const std::filesystem::path& folder);
  void RemoveGameFolder(size_t index);
  void LaunchGame(const std::filesystem::path& path);
  bool is_launching() const { return launching_; }

 protected:
  void OnDestroy() override;

 private:
  class WindowListener final : public ui::WindowListener {
   public:
    explicit WindowListener(UWPEmulatorApp& app) : app_(app) {}
    void OnClosing(ui::UIEvent& e) override;

   private:
    UWPEmulatorApp& app_;
  };

  static std::unique_ptr<apu::AudioSystem> CreateAudioSystem(
      cpu::Processor* processor);
  static std::unique_ptr<gpu::GraphicsSystem> CreateGraphicsSystem();
  static std::vector<std::unique_ptr<hid::InputDriver>> CreateInputDrivers(
      ui::Window* window);

  void EmulatorThread();
  void EnsureProfile();

  void SetupGraphicsSystemPresenterPainting();
  void ShutdownGraphicsSystemPresenterPainting();
  void ApplyDisplayConfigForCvars();

  void LaunchGameFromUIThread(const std::filesystem::path& path);

  void ShowGameList();
  void ShowError(const std::string& title, const std::string& message);

  void LoadGameFolders();
  void SaveGameFolders();

  WindowListener window_listener_;

  std::unique_ptr<Emulator> emulator_;
  std::unique_ptr<ui::Window> window_;
  std::unique_ptr<ui::ImGuiDrawer> imgui_drawer_;
  std::unique_ptr<ui::ImmediateDrawer> immediate_drawer_;
  ui::Presenter* presenter_painting_ = nullptr;

  std::filesystem::path launch_path_;
  std::vector<std::filesystem::path> game_folders_;
  std::vector<GameEntry> games_;
  GameListDialog* game_list_dialog_ = nullptr;
  bool launching_ = false;

  std::unique_ptr<xe::threading::Event> emulator_thread_event_;
  std::thread emulator_thread_;
};

}  // namespace uwp
}  // namespace app
}  // namespace xe

#endif  // XENIA_APP_UWP_UWP_EMULATOR_APP_H_
