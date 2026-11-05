#ifndef MOUSE_CURSOR_DETECTOR_CORE_STARTUP_MANAGER_H_
#define MOUSE_CURSOR_DETECTOR_CORE_STARTUP_MANAGER_H_

#include <windows.h>

#include <string>

namespace mouse_cursor_detector {
struct StartupState {
  bool registered = false;
  bool current_path = false;
  LSTATUS error = ERROR_SUCCESS;
};
std::wstring ExecutablePath();
StartupState QueryStartup();
LSTATUS SetStartupEnabled(bool enabled);
}  // namespace mouse_cursor_detector

#endif  // MOUSE_CURSOR_DETECTOR_CORE_STARTUP_MANAGER_H_
