#ifndef MOUSE_CURSOR_DETECTOR_CORE_HOOK_MANAGER_H_
#define MOUSE_CURSOR_DETECTOR_CORE_HOOK_MANAGER_H_

#include <windows.h>

#include <mutex>
#include <thread>

#include "mouse_cursor_detector/overlay/native_cursor_window.h"
#include "mouse_cursor_detector/settings/settings_model.h"

namespace mouse_cursor_detector {
class HookManager {
 public:
  HookManager() = default;
  HookManager(const HookManager&) = delete;
  HookManager& operator=(const HookManager&) = delete;
  ~HookManager() { Stop(); }
  bool Start(HWND notification_window);
  void Stop();
  bool ApplyState(const Settings& settings, bool visible, bool restart = false);

 private:
  static LRESULT CALLBACK MouseProc(int code, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK CommandProc(HWND window, UINT message, WPARAM wparam,
                                      LPARAM lparam);
  static void CALLBACK ForegroundProc(HWINEVENTHOOK hook, DWORD event,
                                      HWND window, LONG object, LONG child,
                                      DWORD thread, DWORD time);
  void ThreadMain();
  void ApplyPendingState();
  void RefreshDpi();
  void Maintain();
  void Animate();
  void Disable();
  void Fail(DWORD error);
  bool InstallHook();

  std::thread thread_;
  std::mutex mutex_;
  HWND command_window_ = nullptr;  // Protected by mutex_.
  Settings pending_settings_;      // Protected by mutex_.
  bool pending_visible_ = false;   // Protected by mutex_.
  bool pending_restart_ = false;   // Protected by mutex_.
  bool command_pending_ = false;   // Protected by mutex_.
  bool stop_requested_ = false;    // Protected by mutex_.
  HANDLE ready_ = nullptr;
  HWND notification_window_ = nullptr;

  // The following fields are used only on the hook thread.
  NativeCursorWindow* overlay_ = nullptr;
  HHOOK mouse_hook_ = nullptr;
  HWINEVENTHOOK foreground_hook_ = nullptr;
  Settings settings_;
  POINT last_position_{};
  ULONGLONG last_event_ms_ = 0;
  ULONGLONG last_reinstall_ms_ = 0;
  UINT dpi_ = 0;
  bool visible_ = false;
  bool dpi_pending_ = false;
  ULONGLONG animation_start_ms_ = 0;
  size_t frame_index_ = 0;
};
}  // namespace mouse_cursor_detector

#endif  // MOUSE_CURSOR_DETECTOR_CORE_HOOK_MANAGER_H_
