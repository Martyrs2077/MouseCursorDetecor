cmake_minimum_required(VERSION 3.24)
project(MouseCursorDetector VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MINGW)
  message(FATAL_ERROR "Use Windows with MSYS2 UCRT64 MinGW-w64 GCC.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)

set(APP_DIR src/mouse_cursor_detector)
add_executable(MouseCursorDetector WIN32
  ${APP_DIR}/program.cc
  ${APP_DIR}/app.cc
  ${APP_DIR}/core/hook_manager.cc
  ${APP_DIR}/core/hotkey_manager.cc
  ${APP_DIR}/core/startup_manager.cc
  ${APP_DIR}/overlay/native_cursor_window.cc
  ${APP_DIR}/settings/settings_store.cc
  ${APP_DIR}/settings/portable_settings.cc
  ${APP_DIR}/settings/localization.cc
  ${APP_DIR}/media/image_asset.cc
  ${APP_DIR}/settings/settings_window.cc
  ${APP_DIR}/settings/settings_themes.cc
  ${APP_DIR}/settings/settings_materials.cc
  ${APP_DIR}/tray/tray_icon.cc
  ${APP_DIR}/resources/app.rc
)
target_include_directories(MouseCursorDetector PRIVATE src)

set_property(SOURCE ${APP_DIR}/resources/app.rc APPEND PROPERTY OBJECT_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/${APP_DIR}/resources/app.ico")
target_compile_definitions(MouseCursorDetector PRIVATE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
target_compile_options(MouseCursorDetector PRIVATE
  -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
target_include_directories(MouseCursorDetector PRIVATE ${APP_DIR}/resources)
target_link_libraries(MouseCursorDetector PRIVATE
  Threads::Threads user32 gdi32 gdiplus shell32 advapi32 comctl32 comdlg32
  shcore wtsapi32 windowscodecs ole32 uuid)
target_link_options(MouseCursorDetector PRIVATE
  -municode -static -static-libgcc -static-libstdc++)
install(TARGETS MouseCursorDetector RUNTIME DESTINATION .)
