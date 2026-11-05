#ifndef MOUSE_CURSOR_DETECTOR_CORE_HOTKEY_MANAGER_H_
#define MOUSE_CURSOR_DETECTOR_CORE_HOTKEY_MANAGER_H_

#include <windows.h>

#include <array>

namespace mouse_cursor_detector {
class HotkeyManager {
 public:
  ~HotkeyManager() { Reset(); }
  bool Replace(HWND owner, UINT modifiers, UINT vk, UINT secondary_vk);
  void Reset();
  bool ProcessInput(HRAWINPUT input);
  void RefreshKeyState();

 private:
  UINT Modifiers() const;
  HWND owner_ = nullptr;
  std::array<bool, 256> pressed_{};
  UINT modifiers_ = 0;
  UINT vk_ = 0;
  UINT secondary_vk_ = 0;
  bool chord_active_ = false;
};
}  // namespace mouse_cursor_detector

#endif  // MOUSE_CURSOR_DETECTOR_CORE_HOTKEY_MANAGER_H_
