/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <memory>

#include "xenia/base/logging.h"
#include "xenia/ui/file_picker.h"

namespace xe {
namespace ui {

// The WinRT pickers are asynchronous and need the UI thread to keep running,
// while FilePicker::Show is synchronous and is called on the UI thread, so
// waiting for one would deadlock. The UWP frontend uses the asynchronous
// pickers in app/uwp/uwp_pickers.h directly.
class UwpFilePicker : public FilePicker {
 public:
  bool Show(Window* parent_window) override {
    XELOGW("FilePicker: synchronous file pickers aren't supported on UWP");
    return false;
  }
};

std::unique_ptr<FilePicker> FilePicker::Create() {
  return std::make_unique<UwpFilePicker>();
}

}  // namespace ui
}  // namespace xe
