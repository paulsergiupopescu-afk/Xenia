# UWP port (Xbox Series X|S Developer Mode)

Build: `python xenia-build.py build --config=Release --uwp --target=xenia-uwp`, then
`tools/uwp/package.ps1` (see `.github/workflows/uwp.yml`). Needs the Visual Studio
"C++ Universal Windows Platform tools" component for the UWP C++ runtime libraries.

How it fits together:

- `XENIA_UWP` (CMake) defines `XE_PLATFORM_WINUWP`, builds no wxWidgets and adds
  `src/xenia/app/uwp` (an ImGui-only frontend) instead of `src/xenia/app`. The libraries are
  ordinary desktop static libraries; only `xenia-uwp` is linked as an app container against
  `WindowsApp.lib` and the app CRT, so desktop-only APIs surface as link errors.
- `*_uwp.cc` / `*_uwp.h` files are compiled only in that configuration
  (`xe_platform_sources`): the `CoreWindow` window, surface and app context in `src/xenia/ui`,
  the LocalState helpers in `src/xenia/base`.
- `src/xenia/hid/xinput`: controllers through `xinputuap.dll`; SDL can't enumerate devices in an
  app container.

Open issue: guest shaders. UWP on the Xbox has Direct3D 12 with Shader Model 6.4 and no Agility
SDK, while the D3D12 backend lowers all guest resources to SM 6.6 `ResourceDescriptorHeap`
indexing (Mesa `spirv_to_dxil` with `lower_to_bindless`). The plan is to lower bindless
resources to unbounded descriptor arrays (`createHandle` with a dynamic index, SM 5.1+) in Mesa
when the shader model is below 6.6, keeping the index buffers and heaps the backend already
uses.
