#ifndef MOUSE_CURSOR_DETECTOR_INTEROP_WINDOW_MESSAGES_H_
#define MOUSE_CURSOR_DETECTOR_INTEROP_WINDOW_MESSAGES_H_

#include <windows.h>

namespace mouse_cursor_detector {
inline constexpr wchar_t kControlClass[] = L"MouseCursorDetector.Control.v1";
inline constexpr UINT kTrayMessage = WM_APP + 1;
inline constexpr UINT kApplyState = WM_APP + 2;
inline constexpr UINT kRefreshDpi = WM_APP + 3;
inline constexpr UINT kWorkerError = WM_APP + 4;
inline constexpr UINT kWorkerState = WM_APP + 5;
inline constexpr UINT kOpenSettings = WM_APP + 6;
inline constexpr int kSettingsCommand = 101;
inline constexpr int kStartupCommand = 102;
inline constexpr int kAboutCommand = 103;
inline constexpr int kQuitCommand = 104;
}  // namespace mouse_cursor_detector

#endif  // MOUSE_CURSOR_DETECTOR_INTEROP_WINDOW_MESSAGES_H_
