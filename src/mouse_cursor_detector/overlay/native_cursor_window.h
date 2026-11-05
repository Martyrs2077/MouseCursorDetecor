#ifndef MOUSE_CURSOR_DETECTOR_OVERLAY_NATIVE_CURSOR_WINDOW_H_
#define MOUSE_CURSOR_DETECTOR_OVERLAY_NATIVE_CURSOR_WINDOW_H_

#include <windows.h>

#include <cstddef>

#include "mouse_cursor_detector/settings/settings_model.h"

namespace mouse_cursor_detector {
// All operations, including destruction, belong to the hook thread.
class NativeCursorWindow {
 public:
  NativeCursorWindow() = default;
  NativeCursorWindow(const NativeCursorWindow&) = delete;
  NativeCursorWindow& operator=(const NativeCursorWindow&) = delete;
  ~NativeCursorWindow();

  bool Create();
  bool ApplyStyle(const Settings& settings, UINT dpi, size_t frame_index = 0);
  void FollowCursor(POINT point);
  void SetVisible(bool visible);
  void ReassertTopmost();
  HWND handle() const { return window_; }

 private:
  void ReleaseSurface();
  HWND window_ = nullptr;
  HDC memory_dc_ = nullptr;
  HBITMAP bitmap_ = nullptr;
  HGDIOBJ original_bitmap_ = nullptr;
  int width_px_ = 0;
  int height_px_ = 0;
};
}  // namespace mouse_cursor_detector

#endif  // MOUSE_CURSOR_DETECTOR_OVERLAY_NATIVE_CURSOR_WINDOW_H_
