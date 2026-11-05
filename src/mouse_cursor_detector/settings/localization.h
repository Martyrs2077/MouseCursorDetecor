#ifndef MOUSE_CURSOR_DETECTOR_SETTINGS_LOCALIZATION_H_
#define MOUSE_CURSOR_DETECTOR_SETTINGS_LOCALIZATION_H_

#include <cstdint>

namespace mouse_cursor_detector {
enum class UiLanguage : uint32_t { kChinese = 0, kEnglish = 1 };

void SetUiLanguage(UiLanguage language);
UiLanguage CurrentUiLanguage();
// Translate application text only. User names, paths and license text are data.
const wchar_t* Tr(const wchar_t* chinese);
}  // namespace mouse_cursor_detector

#endif  // MOUSE_CURSOR_DETECTOR_SETTINGS_LOCALIZATION_H_
