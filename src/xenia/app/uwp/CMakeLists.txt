# The UWP app for Xbox Series X|S Developer Mode (XENIA_UWP).
#
# The libraries are regular desktop static libraries; only this executable is
# linked as an app container, against WindowsApp.lib and the UWP flavor of the
# Visual C++ runtime (vcruntime140_app.dll etc. from the Microsoft.VCLibs
# framework package). tools/uwp/package.ps1 lays out and packs the .msix.

# Only APIs available to app containers may be linked, so drop the desktop
# import libraries CMake adds by default (kernel32, user32, advapi32...) in this
# directory, WindowsApp.lib replaces them. Build-time host tools elsewhere keep
# them.
set(CMAKE_C_STANDARD_LIBRARIES "uuid.lib")
set(CMAKE_CXX_STANDARD_LIBRARIES "uuid.lib")

add_executable(xenia-uwp WIN32
  ${CMAKE_CURRENT_SOURCE_DIR}/uwp_emulator_app.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/uwp_emulator_app.h
  ${CMAKE_CURRENT_SOURCE_DIR}/uwp_main.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/uwp_pickers.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/uwp_pickers.h
)
set_target_properties(xenia-uwp PROPERTIES OUTPUT_NAME "xenia_edge_uwp")
# Also drop the desktop libraries from the global link_libraries.
set_target_properties(xenia-uwp PROPERTIES LINK_LIBRARIES "")

target_compile_definitions(xenia-uwp PRIVATE
  XBYAK_NO_OP_NAMES
  XBYAK_ENABLE_OMITTED_OPERAND
)

target_link_libraries(xenia-uwp PRIVATE
  xenia-apu
  xenia-apu-nop
  xenia-apu-xaudio2
  xenia-base
  xenia-core
  xenia-cpu
  xenia-cpu-backend-x64
  xenia-gpu
  xenia-gpu-d3d12
  xenia-hid
  xenia-hid-nop
  xenia-hid-xinput
  xenia-kernel
  xenia-patcher
  xenia-ui
  xenia-ui-d3d12
  xenia-vfs
  aes_128
  capstone
  fmt
  dxbc
  imgui
  libavcodec
  libavutil
  mspack
  snappy
  xxhash
)

# The UWP CRT import libraries live next to the desktop ones, in lib/<arch>/
# store, and come with the Visual Studio "C++ Universal Windows Platform
# tools" component.
get_filename_component(_xe_vc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
get_filename_component(_xe_vc_tools_dir "${_xe_vc_bin_dir}/../../.." ABSOLUTE)
set(_xe_vc_store_lib_dir "${_xe_vc_tools_dir}/lib/x64/store")
if(NOT EXISTS "${_xe_vc_store_lib_dir}/msvcrt.lib")
  message(FATAL_ERROR
    "The UWP Visual C++ runtime libraries weren't found in "
    "${_xe_vc_store_lib_dir}. Install the Visual Studio \"C++ Universal "
    "Windows Platform tools\" component.")
endif()

target_link_options(xenia-uwp PRIVATE
  /APPCONTAINER
  /SUBSYSTEM:WINDOWS
  /ENTRY:wWinMainCRTStartup
  # Replace the desktop CRT with the app one.
  /NODEFAULTLIB:msvcrt.lib
  /NODEFAULTLIB:vcruntime.lib
  /NODEFAULTLIB:msvcprt.lib
  # Libraries requested by #pragma comment(lib) in desktop-oriented code.
  /NODEFAULTLIB:kernel32.lib
  /NODEFAULTLIB:user32.lib
  /NODEFAULTLIB:advapi32.lib
  /NODEFAULTLIB:shell32.lib
)
target_link_libraries(xenia-uwp PRIVATE
  "${_xe_vc_store_lib_dir}/msvcrt.lib"
  "${_xe_vc_store_lib_dir}/vcruntime.lib"
  "${_xe_vc_store_lib_dir}/msvcprt.lib"
  WindowsApp
  dxguid
)

xe_target_defaults(xenia-uwp)
