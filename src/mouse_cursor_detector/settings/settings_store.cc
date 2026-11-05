#include "mouse_cursor_detector/settings/settings_store.h"

#include <array>

namespace mouse_cursor_detector {
namespace {
constexpr wchar_t kSettingsKey[] = L"Software\\MouseCursorDetector";
constexpr uint32_t kMagic = 0x4d434431;
constexpr uint32_t kVersion = 3;
// Explicit DWORD fields: no structure padding or pointers are persisted.
using Record = std::array<DWORD, 15>;
}  // namespace

SettingsLoadResult LoadLegacySettings() {
  SettingsLoadResult result;
  Record record{};
  DWORD bytes = sizeof(record);
  const LSTATUS status =
      RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, L"Settings",
                   RRF_RT_REG_BINARY, nullptr, record.data(), &bytes);
  if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) {
    return result;
  }
  result.first_run = false;
  const bool legacy_record =
      (record[1] == 1 || record[1] == 2) && bytes == 14 * sizeof(DWORD);
  const bool current_record = record[1] == kVersion && bytes == sizeof(record);
  if (status != ERROR_SUCCESS || record[0] != kMagic ||
      (!legacy_record && !current_record)) {
    result.invalid = true;
    return result;
  }
  auto& settings = result.settings;
  settings.mode = static_cast<IndicatorMode>(record[2]);
  if (settings.mode != IndicatorMode::kHighlight &&
      settings.mode != IndicatorMode::kRing)
    settings.mode = IndicatorMode::kRing;
  settings.ring = {record[3], static_cast<int>(record[4]),
                   static_cast<int>(record[5]), static_cast<int>(record[6])};
  settings.highlight = {record[7], static_cast<int>(record[8]), 1,
                        static_cast<int>(record[9])};
  settings.hotkey_modifiers = record[10];
  settings.hotkey_vk = record[11];
  settings.hotkey_secondary_vk = record[1] >= 3 ? record[14] : 0;
  // Ignore the legacy enabled-on-launch field. Visibility is session-only.
  if (record[1] >= 2) settings.hold_seconds = static_cast<int>(record[13]);
  settings.Normalize();
  return result;
}

}  // namespace mouse_cursor_detector
