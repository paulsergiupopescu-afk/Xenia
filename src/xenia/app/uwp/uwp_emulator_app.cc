/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/app/uwp/uwp_emulator_app.h"

#include <algorithm>
#include <cstring>
#include <sstream>

#include "third_party/imgui/imgui.h"
#include "xenia/app/uwp/uwp_pickers.h"
#include "xenia/apu/nop/nop_audio_system.h"
#include "xenia/apu/xaudio2/xaudio2_audio_system.h"
#include "xenia/base/cvar.h"
#include "xenia/base/filesystem.h"
#include "xenia/base/logging.h"
#include "xenia/base/platform_amd64.h"
#include "xenia/base/platform_uwp.h"
#include "xenia/base/utf8.h"
#include "xenia/config.h"
#include "xenia/gpu/command_processor.h"
#include "xenia/gpu/d3d12/d3d12_graphics_system.h"
#include "xenia/gpu/graphics_system.h"
#include "xenia/hid/nop/nop_hid.h"
#include "xenia/hid/xinput/xinput_hid.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/xam/profile_manager.h"
#include "xenia/kernel/xam/xam_module.h"
#include "xenia/kernel/xam/xam_state.h"
#include "xenia/ui/imgui_dialog.h"
#include "xenia/vfs/virtual_file_system.h"

// Backends. The Xbox exposes only Direct3D 12, XAudio2 and XInput to UWP apps,
// these exist so the config file has the same keys as on the desktop.
DEFINE_string(apu, "xaudio2", "Audio system. Use: [xaudio2, nop]", "APU");
DEFINE_string(gpu, "d3d12", "Graphics system. Use: [d3d12, null]", "GPU");
DEFINE_string(hid, "xinput", "Input system. Use: [xinput, nop]", "HID");

DECLARE_bool(in_process_title_relaunch);

DEFINE_bool(mount_scratch, false, "Enable scratch mount", "Storage");
DEFINE_bool(mount_cache, true, "Enable cache mount", "Storage");
DEFINE_bool(mount_memory_unit, false, "Enable memory unit (MU) mount",
            "Storage");

DEFINE_string(uwp_game_folders, "",
              "Folders scanned for games by the UWP game list, separated by "
              "semicolons.",
              "UWP");

DEFINE_string(
    postprocess_antialiasing, "",
    "Post-processing anti-aliasing effect to apply to the image output of the "
    "game.\n"
    "Use: [none, fxaa, fxaa_extreme]",
    "Display");
DEFINE_string(
    postprocess_scaling_and_sharpening, "fsr",
    "Post-processing effect to use for resampling and/or sharpening of the "
    "final display output.\n"
    "Use: [bilinear, cas, fsr]",
    "Display");
DEFINE_double(
    postprocess_ffx_cas_additional_sharpness,
    xe::ui::Presenter::GuestOutputPaintConfig::kCasAdditionalSharpnessDefault,
    "Additional sharpness for AMD FidelityFX Contrast Adaptive Sharpening "
    "(CAS), from 0 to 1.\n"
    "Higher is sharper.",
    "Display");
DEFINE_uint32(
    postprocess_ffx_fsr_max_upsampling_passes,
    xe::ui::Presenter::GuestOutputPaintConfig::kFsrMaxUpscalingPassesMax,
    "Maximum number of upsampling passes performed in AMD FidelityFX Super "
    "Resolution 1.0 (FSR) before falling back to bilinear stretching after the "
    "final pass.",
    "Display");
DEFINE_double(
    postprocess_ffx_fsr_sharpness_reduction,
    xe::ui::Presenter::GuestOutputPaintConfig::kFsrSharpnessReductionDefault,
    "Sharpness reduction for AMD FidelityFX Super Resolution 1.0 (FSR), in "
    "stops.\n"
    "Lower is sharper.",
    "Display");
DEFINE_bool(postprocess_dither, true,
            "Dither the final image output from the internal precision to 8 "
            "bits per channel so gradients are smoother.",
            "Display");

namespace xe {
namespace app {
namespace uwp {

class GameListDialog final : public ui::ImGuiDialog {
 public:
  GameListDialog(ui::ImGuiDrawer* imgui_drawer, UWPEmulatorApp& app)
      : ui::ImGuiDialog(imgui_drawer), app_(app) {}

  void RequestClose() { Close(); }

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  UWPEmulatorApp& app_;
};

void GameListDialog::OnDraw(ImGuiIO& io) {
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(io.DisplaySize);
  if (!ImGui::Begin("Xenia Edge for Xbox##game_list", nullptr,
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::End();
    return;
  }

  ImGui::TextUnformatted("Xenia Edge for Xbox");
  ImGui::Separator();

  if (app_.is_launching()) {
    ImGui::TextUnformatted("Starting the game...");
    ImGui::End();
    return;
  }

  if (ImGui::Button("Open a game file...")) {
    PickFile(app_.app_context(),
             [this](const std::filesystem::path& path) {
               if (!path.empty()) {
                 app_.LaunchGame(path);
               }
             });
  }
  ImGui::SameLine();
  if (ImGui::Button("Add a game folder...")) {
    PickFolder(app_.app_context(),
               [this](const std::filesystem::path& path) {
                 if (!path.empty()) {
                   app_.AddGameFolder(path);
                 }
               });
  }
  ImGui::SameLine();
  if (ImGui::Button("Refresh")) {
    app_.RefreshGames();
  }

  const auto& folders = app_.game_folders();
  if (!folders.empty() && ImGui::CollapsingHeader("Game folders")) {
    for (size_t i = 0; i < folders.size(); ++i) {
      ImGui::PushID(int(i));
      if (ImGui::SmallButton("Remove")) {
        ImGui::PopID();
        app_.RemoveGameFolder(i);
        break;
      }
      ImGui::SameLine();
      ImGui::TextUnformatted(xe::path_to_utf8(folders[i]).c_str());
      ImGui::PopID();
    }
  }

  ImGui::Separator();
  const auto& games = app_.games();
  if (games.empty()) {
    ImGui::TextWrapped(
        "No games found. Add a folder with your games (extracted folders "
        "with default.xex, .iso, .zar or Games on Demand packages), or open "
        "a game file directly.");
  } else {
    ImGui::Text("Games (%zu):", games.size());
    if (ImGui::BeginChild("##games", ImVec2(0.0f, 0.0f), true)) {
      for (size_t i = 0; i < games.size(); ++i) {
        ImGui::PushID(int(i));
        if (ImGui::Selectable(games[i].name.c_str())) {
          app_.LaunchGame(games[i].path);
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemFocused()) {
          ImGui::SetTooltip("%s", xe::path_to_utf8(games[i].path).c_str());
        }
        ImGui::PopID();
      }
    }
    ImGui::EndChild();
  }

  ImGui::End();
}

void UWPEmulatorApp::WindowListener::OnClosing(ui::UIEvent& e) {
  app_.app_context().QuitFromUIThread();
}

UWPEmulatorApp::UWPEmulatorApp(ui::WindowedAppContext& app_context)
    : ui::WindowedApp(app_context, "xenia"), window_listener_(*this) {}

UWPEmulatorApp::~UWPEmulatorApp() = default;

std::unique_ptr<apu::AudioSystem> UWPEmulatorApp::CreateAudioSystem(
    cpu::Processor* processor) {
  if (cvars::apu != "nop") {
    auto audio_system = apu::xaudio2::XAudio2AudioSystem::Create(processor);
    if (audio_system) {
      return audio_system;
    }
  }
  return apu::nop::NopAudioSystem::Create(processor);
}

std::unique_ptr<gpu::GraphicsSystem> UWPEmulatorApp::CreateGraphicsSystem() {
  if (!gpu::d3d12::D3D12GraphicsSystem::IsAvailable()) {
    xe::FatalError("Direct3D 12 is unavailable");
  }
  return std::make_unique<gpu::d3d12::D3D12GraphicsSystem>();
}

std::vector<std::unique_ptr<hid::InputDriver>>
UWPEmulatorApp::CreateInputDrivers(ui::Window* window) {
  std::vector<std::unique_ptr<hid::InputDriver>> drivers;
  if (cvars::hid != "nop") {
    auto driver = hid::xinput::Create(window, kZOrderHidInput);
    if (XSUCCEEDED(driver->Setup())) {
      drivers.emplace_back(std::move(driver));
    }
  }
  if (drivers.empty()) {
    drivers.emplace_back(hid::nop::Create(window, kZOrderHidInput));
  }
  return drivers;
}

bool UWPEmulatorApp::OnInitialize() {
  // The package folder is read-only, everything is stored in LocalState.
  std::filesystem::path storage_root = xe::uwp::GetLocalStateFolder();
  XELOGI("Storage root: {}", storage_root);
  config::SetupConfig(storage_root);
  // Title-to-title launches can't spawn a new process in UWP.
  cvars::in_process_title_relaunch = true;

#if XE_ARCH_AMD64 == 1
  amd64::InitFeatureFlags();
#endif

  std::filesystem::path content_root = storage_root / "content";
  std::filesystem::path cache_root = storage_root / "cache_host";
  emulator_ =
      std::make_unique<Emulator>("", storage_root, content_root, cache_root);

  auto res = gpu::GraphicsSystem::GetInternalDisplayResolution();
  window_ = ui::Window::Create(app_context(), "Xenia Edge", res.first,
                               res.second);
  window_->AddListener(&window_listener_);
  imgui_drawer_ = std::make_unique<ui::ImGuiDrawer>(window_.get(), kZOrderImGui);
  if (!window_->Open()) {
    XELOGE("Failed to open the window");
    return false;
  }

  LoadGameFolders();
  RefreshGames();

  emulator_thread_event_ = xe::threading::Event::CreateAutoResetEvent(false);
  emulator_thread_ = std::thread(&UWPEmulatorApp::EmulatorThread, this);
  return true;
}

void UWPEmulatorApp::OnDestroy() {
  // As on the desktop, there's no clean emulator shutdown yet.
  XELOGI("Exiting");
  xe::FlushLog();
  std::quick_exit(EXIT_SUCCESS);
}

void UWPEmulatorApp::EmulatorThread() {
  xe::threading::set_name("Emulator");

  X_STATUS result =
      emulator_->Setup(window_.get(), imgui_drawer_.get(), true,
                       CreateAudioSystem, CreateGraphicsSystem,
                       CreateInputDrivers);
  if (XFAILED(result)) {
    XELOGE("Failed to setup the emulator: {:08X}", result);
    app_context().CallInUIThread([this, result]() {
      ShowError("Error", fmt::format("Failed to set up the emulator: {:08X}",
                                     uint32_t(result)));
    });
    return;
  }
  emulator_->MountStandardDrives();
  EnsureProfile();

  emulator_->on_launch.AddListener([this](auto title_id, const auto& title) {
    // Title-to-title launches recreate the graphics system.
    app_context().CallInUIThread([this]() {
      SetupGraphicsSystemPresenterPainting();
      launching_ = false;
    });
    emulator_thread_event_->Set();
  });
  emulator_->on_before_shutdown.AddListener([this]() {
    // Detach from the presenter while the graphics system is still alive.
    app_context().CallInUIThreadSynchronous(
        [this]() { ShutdownGraphicsSystemPresenterPainting(); });
  });

  // The UI is drawn by the presenter of the graphics system, so bring the
  // subsystems up right away rather than at the first launch like on the
  // desktop, where the game list is a wx window.
  result = emulator_->SetupSubsystems();
  if (XFAILED(result)) {
    XELOGE("Failed to setup the emulator subsystems: {:08X}", result);
    return;
  }
  app_context().CallInUIThreadSynchronous([this]() {
    SetupGraphicsSystemPresenterPainting();
    if (!launch_path_.empty()) {
      LaunchGame(launch_path_);
    } else {
      ShowGameList();
    }
  });

  while (true) {
    xe::threading::Wait(emulator_thread_event_.get(), false);
    emulator_->WaitUntilExit();
  }
}

void UWPEmulatorApp::EnsureProfile() {
  auto* profile_manager =
      emulator_->kernel_state()->xam_state()->profile_manager();
  if (!profile_manager || profile_manager->GetAccountCount()) {
    return;
  }
  // Many games, Need for Speed: The Run among them, require a signed-in
  // profile, and there's no keyboard to type a gamertag with on the console.
  XELOGI("No profiles, creating a default one");
  if (!profile_manager->CreateProfile("Player", /*autologin=*/true)) {
    XELOGE("Failed to create the default profile");
  }
}

void UWPEmulatorApp::SetupGraphicsSystemPresenterPainting() {
  gpu::GraphicsSystem* graphics_system = emulator_->graphics_system();
  ui::Presenter* presenter =
      graphics_system ? graphics_system->presenter() : nullptr;
  if (presenter && presenter_painting_ == presenter) {
    return;
  }
  ShutdownGraphicsSystemPresenterPainting();
  if (!presenter) {
    return;
  }
  ApplyDisplayConfigForCvars();
  window_->SetPresenter(presenter);
  presenter_painting_ = presenter;
  immediate_drawer_ = graphics_system->provider()->CreateImmediateDrawer();
  if (immediate_drawer_) {
    immediate_drawer_->SetPresenter(presenter);
    imgui_drawer_->SetPresenterAndImmediateDrawer(presenter,
                                                  immediate_drawer_.get());
  }
}

void UWPEmulatorApp::ShutdownGraphicsSystemPresenterPainting() {
  imgui_drawer_->SetPresenterAndImmediateDrawer(nullptr, nullptr);
  immediate_drawer_.reset();
  window_->SetPresenter(nullptr);
  presenter_painting_ = nullptr;
}

void UWPEmulatorApp::ApplyDisplayConfigForCvars() {
  gpu::GraphicsSystem* graphics_system = emulator_->graphics_system();
  if (!graphics_system) {
    return;
  }
  if (gpu::CommandProcessor* command_processor =
          graphics_system->command_processor()) {
    gpu::CommandProcessor::SwapPostEffect effect =
        gpu::CommandProcessor::SwapPostEffect::kNone;
    if (cvars::postprocess_antialiasing == "fxaa") {
      effect = gpu::CommandProcessor::SwapPostEffect::kFxaa;
    } else if (cvars::postprocess_antialiasing == "fxaa_extreme") {
      effect = gpu::CommandProcessor::SwapPostEffect::kFxaaExtreme;
    }
    command_processor->SetDesiredSwapPostEffect(effect);
  }
  if (ui::Presenter* presenter = graphics_system->presenter()) {
    ui::Presenter::GuestOutputPaintConfig paint_config;
    paint_config.SetAllowOverscanCutoff(true);
    ui::Presenter::GuestOutputPaintConfig::Effect paint_effect =
        ui::Presenter::GuestOutputPaintConfig::Effect::kBilinear;
    if (cvars::postprocess_scaling_and_sharpening == "cas") {
      paint_effect = ui::Presenter::GuestOutputPaintConfig::Effect::kCas;
    } else if (cvars::postprocess_scaling_and_sharpening == "fsr") {
      paint_effect = ui::Presenter::GuestOutputPaintConfig::Effect::kFsr;
    }
    paint_config.SetEffect(paint_effect);
    paint_config.SetCasAdditionalSharpness(
        float(cvars::postprocess_ffx_cas_additional_sharpness));
    paint_config.SetFsrMaxUpsamplingPasses(
        cvars::postprocess_ffx_fsr_max_upsampling_passes);
    paint_config.SetFsrSharpnessReduction(
        float(cvars::postprocess_ffx_fsr_sharpness_reduction));
    paint_config.SetDither(cvars::postprocess_dither);
    presenter->SetGuestOutputPaintConfigFromUIThread(paint_config);
  }
}

void UWPEmulatorApp::ShowGameList() {
  if (!game_list_dialog_) {
    XELOGI("UWP: Showing the game list");
    game_list_dialog_ = new GameListDialog(imgui_drawer_.get(), *this);
  }
}

void UWPEmulatorApp::ShowError(const std::string& title,
                               const std::string& message) {
  ui::ImGuiDialog::ShowMessageBox(imgui_drawer_.get(), title, message);
}

void UWPEmulatorApp::LoadGameFolders() {
  game_folders_.clear();
  std::stringstream stream(cvars::uwp_game_folders);
  std::string folder;
  while (std::getline(stream, folder, ';')) {
    if (!folder.empty()) {
      game_folders_.push_back(xe::to_path(folder));
    }
  }
}

void UWPEmulatorApp::SaveGameFolders() {
  std::string value;
  for (const auto& folder : game_folders_) {
    value += xe::path_to_utf8(folder);
    value += ';';
  }
  OVERRIDE_PERSIST_string(uwp_game_folders, value);
  config::SaveConfig();
}

void UWPEmulatorApp::AddGameFolder(const std::filesystem::path& folder) {
  if (std::find(game_folders_.cbegin(), game_folders_.cend(), folder) ==
      game_folders_.cend()) {
    game_folders_.push_back(folder);
    SaveGameFolders();
  }
  RefreshGames();
}

void UWPEmulatorApp::RemoveGameFolder(size_t index) {
  if (index < game_folders_.size()) {
    game_folders_.erase(game_folders_.begin() + index);
    SaveGameFolders();
    RefreshGames();
  }
}

// The default.xex of an extracted disc in the folder, in any case, or an empty
// path.
static std::filesystem::path FindDefaultXex(const std::filesystem::path& dir) {
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (xe::utf8::lower_ascii(xe::path_to_utf8(entry.path().filename())) ==
            "default.xex" &&
        entry.is_regular_file(ec)) {
      return entry.path();
    }
  }
  return {};
}

void UWPEmulatorApp::RefreshGames() {
  games_.clear();
  std::vector<std::filesystem::path> roots = game_folders_;
  // Games copied over with the Device Portal file explorer.
  roots.push_back(xe::uwp::GetLocalStateFolder() / "games");
  for (const auto& root : roots) {
    // An extracted disc added as a game folder itself.
    std::filesystem::path root_default_xex = FindDefaultXex(root);
    if (!root_default_xex.empty()) {
      games_.push_back({xe::path_to_utf8(root.filename()), root_default_xex});
      continue;
    }
    std::error_code ec;
    auto it = std::filesystem::recursive_directory_iterator(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
      continue;
    }
    for (; it != std::filesystem::recursive_directory_iterator();
         it.increment(ec)) {
      if (ec) {
        break;
      }
      const std::filesystem::directory_entry& entry = *it;
      if (entry.is_directory(ec)) {
        // An extracted disc is one game named after its folder, and the other
        // executables in it (like the $SystemUpdate ones) aren't games.
        std::filesystem::path default_xex = FindDefaultXex(entry.path());
        if (!default_xex.empty()) {
          games_.push_back(
              {xe::path_to_utf8(entry.path().filename()), default_xex});
          it.disable_recursion_pending();
        }
        continue;
      }
      if (!entry.is_regular_file(ec)) {
        continue;
      }
      const std::filesystem::path& path = entry.path();
      std::string name;
      switch (emulator_->GetFileSignature(path)) {
        case Emulator::FileSignatureType::XEX1:
        case Emulator::FileSignatureType::XEX2:
          name = xe::path_to_utf8(path.stem());
          break;
        case Emulator::FileSignatureType::CON:
        case Emulator::FileSignatureType::LIVE:
        case Emulator::FileSignatureType::PIRS:
        case Emulator::FileSignatureType::XISO:
        case Emulator::FileSignatureType::ZAR:
          name = xe::path_to_utf8(path.stem());
          break;
        default:
          continue;
      }
      games_.push_back({std::move(name), path});
    }
  }
  std::sort(games_.begin(), games_.end(),
            [](const GameEntry& a, const GameEntry& b) {
              return a.name < b.name;
            });
}

void UWPEmulatorApp::LaunchGame(const std::filesystem::path& path) {
  // May be called while drawing the game list, which can't be torn down from
  // inside itself.
  app_context().CallInUIThreadDeferred(
      [this, path]() { LaunchGameFromUIThread(path); });
}

void UWPEmulatorApp::LaunchGameFromUIThread(
    const std::filesystem::path& path) {
  if (launching_ || emulator_->is_title_open()) {
    return;
  }
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    ShowError("Title Launch Failed!",
              fmt::format("The file can't be accessed:\n{}\n\nIf it's on an "
                          "external drive, allow the app to access the file "
                          "system in its settings.",
                          xe::path_to_utf8(path)));
    return;
  }
  launching_ = true;

  // Bring the subsystems back up with the per-game config applied, like
  // EmulatorWindow::RunTitle on the desktop.
  std::filesystem::path abs_path = std::filesystem::absolute(path, ec);
  ShutdownGraphicsSystemPresenterPainting();
  emulator_->ShutdownSubsystems();
  config::ReloadConfig();
  config::LoadGameConfigForFile(abs_path);
  cvars::in_process_title_relaunch = true;
  X_STATUS status = emulator_->SetupSubsystems();
  SetupGraphicsSystemPresenterPainting();
  if (XFAILED(status)) {
    launching_ = false;
    ShowError("Title Launch Failed!",
              fmt::format("Failed to set up the emulator subsystems: {:08X}",
                          uint32_t(status)));
    return;
  }
  if (game_list_dialog_) {
    game_list_dialog_->RequestClose();
    game_list_dialog_ = nullptr;
  }

  // LaunchPath blocks for a while, keep the UI responsive.
  std::thread([this, abs_path]() {
    X_STATUS result = emulator_->LaunchPath(abs_path);
    app_context().CallInUIThread([this, result, abs_path]() {
      if (XFAILED(result)) {
        XELOGE("Failed to launch {}: {:08X}", xe::path_to_utf8(abs_path),
               uint32_t(result));
        launching_ = false;
        emulator_->file_system()->Clear();
        ShowError("Title Launch Failed!",
                  "Failed to launch the title.\n\nCheck xenia.log in the "
                  "app's LocalState folder for details.");
        ShowGameList();
        return;
      }
      if (auto xam =
              emulator_->kernel_state()
                  ->GetKernelModule<kernel::xam::XamModule>("xam.xex")) {
        xam->loader_data().host_path = xe::path_to_utf8(abs_path);
      }
    });
  }).detach();
}

}  // namespace uwp
}  // namespace app
}  // namespace xe
