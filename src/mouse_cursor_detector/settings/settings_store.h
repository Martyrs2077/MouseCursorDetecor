#ifndef MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_STORE_H_
#define MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_STORE_H_

#include "mouse_cursor_detector/settings/settings_model.h"

namespace mouse_cursor_detector {
struct SettingsLoadResult {
  Settings settings;
  bool first_run = true;
  bool invalid = false;
  bool needs_save = false;
  std::wstring style_error;
};
SettingsLoadResult LoadSettings();
LSTATUS SaveSettings(const Settings& settings);
}  // namespace mouse_cursor_detector

#endif  // MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_STORE_H_
