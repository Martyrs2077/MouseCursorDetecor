#ifndef MOUSE_CURSOR_DETECTOR_APP_H_
#define MOUSE_CURSOR_DETECTOR_APP_H_

#include <windows.h>

#include <memory>

#include "mouse_cursor_detector/core/hook_manager.h"
#include "mouse_cursor_detector/core/hotkey_manager.h"
#include "mouse_cursor_detector/settings/settings_model.h"
#include "mouse_cursor_detector/settings/settings_window.h"
#include "mouse_cursor_detector/tray/tray_icon.h"

namespace mouse_cursor_detector {
class App {
 public:
  ~App();
  int Run(bool autostart);

 private:
  static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam,
                                     LPARAM lparam);
  LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
  void TriggerIndicator();
  void HideIndicator();
  void ExpireIndicatorIfNeeded();
  void ShowSettings();
  void ToggleStartup();
  void Preview(const Settings& settings);
  bool Commit(const Settings& settings);
  void Report(const wchar_t* context, DWORD error);

  HWND window_ = nullptr;
  UINT taskbar_created_ = 0;
  Settings settings_;
  Settings displayed_settings_;
  HookManager hooks_;
  HotkeyManager hotkey_;
  TrayIcon tray_;
  std::unique_ptr<SettingsWindow> settings_window_;
  bool requested_visible_ = false;
  bool actual_visible_ = false;
  bool session_locked_ = false;
  bool worker_ready_ = false;
  ULONGLONG hide_deadline_ms_ = 0;
};
}  // namespace mouse_cursor_detector

#endif  // MOUSE_CURSOR_DETECTOR_APP_H_
