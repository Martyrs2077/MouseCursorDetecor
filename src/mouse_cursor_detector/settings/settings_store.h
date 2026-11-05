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
struct StyleLibraryResult {
  bool success = false;
  std::vector<UserStyle> styles;
  std::vector<std::shared_ptr<const ImageAsset>> thumbnails;
  std::vector<std::wstring> missing_names;
  std::shared_ptr<const ImageAsset> selected_asset;
  std::wstring error;
};

// Missing maps use the legacy in-memory library until the next explicit save.
bool LoadStyleLibrary(std::vector<UserStyle>* styles, std::wstring* error);
LSTATUS SaveStyleLibrary(const std::vector<UserStyle>& styles);
StyleLibraryResult RefreshStyleLibrary(const std::vector<UserStyle>& styles,
                                       const std::vector<UserStyle>& baseline,
                                       const std::wstring& selected_name);
std::shared_ptr<const ImageAsset> LoadMappedStyle(
    const std::vector<UserStyle>& styles, const std::wstring& name,
    std::wstring* error, bool thumbnail = false);
SettingsLoadResult LoadSettings();
LSTATUS SaveSettings(const Settings& settings);
}  // namespace mouse_cursor_detector

#endif  // MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_STORE_H_
