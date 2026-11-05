#ifndef MOUSE_CURSOR_DETECTOR_TRAY_TRAY_ICON_H_
#define MOUSE_CURSOR_DETECTOR_TRAY_TRAY_ICON_H_

#include <windows.h>

#include <shellapi.h>

#include "mouse_cursor_detector/settings/settings_model.h"

namespace mouse_cursor_detector {
class TrayIcon {
 public:
  ~TrayIcon();
  bool Add(HWND owner);
  void Remove();
  void Update(bool enabled, IndicatorMode mode);
  void Warn(const wchar_t* message);
  void ShowMenu(HWND owner, bool startup_enabled);

 private:
  NOTIFYICONDATAW data_{};
  bool added_ = false;
};
}  // namespace mouse_cursor_detector

#endif  // MOUSE_CURSOR_DETECTOR_TRAY_TRAY_ICON_H_
