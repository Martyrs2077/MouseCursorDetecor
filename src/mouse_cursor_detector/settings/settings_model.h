#ifndef MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_MODEL_H_
#define MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_MODEL_H_

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mouse_cursor_detector/media/image_asset.h"
#include "mouse_cursor_detector/settings/localization.h"

namespace mouse_cursor_detector {

enum class IndicatorMode : uint32_t {
  kHighlight = 0,
  kRing = 1,
  kImage = 2,
  kGif = 3
};

enum class UiTheme : uint32_t {
  kNative = 0,
  kDark = 1,
  kPaper = 2,
  kMinimal = 3
};

struct IndicatorStyle {
  COLORREF color = RGB(2, 255, 96);
  int radius_dip = 36;
  int thickness_dip = 7;
  int opacity_percent = 90;
};

struct UserStyle {
  std::wstring name;
  std::wstring relative_path;
};

constexpr size_t kMaxUserStyles = 512;
constexpr size_t kMaxStyleNameLength = 255;
constexpr int kMaxImageOffsetDip = 512;

struct ImageOffset {
  int x_dip = 0;
  int y_dip = 0;
};

inline std::wstring TrimStyleName(const std::wstring& name) {
  const auto first = name.find_first_not_of(L" \t\r\n");
  if (first == std::wstring::npos) return {};
  return name.substr(first, name.find_last_not_of(L" \t\r\n") - first + 1);
}

inline bool IsValidStyleName(const std::wstring& name) {
  return !name.empty() && name.size() <= kMaxStyleNameLength &&
         name == TrimStyleName(name) &&
         std::none_of(name.begin(), name.end(),
                      [](wchar_t c) { return c < 32; });
}

inline bool StyleNamesEqual(const std::wstring& left,
                            const std::wstring& right) {
  return left == right;
}

inline bool HasStyleName(const std::vector<UserStyle>& styles,
                         const std::wstring& name) {
  return std::any_of(styles.begin(), styles.end(), [&](const UserStyle& style) {
    return StyleNamesEqual(style.name, name);
  });
}

struct Settings {
  UiLanguage ui_language = UiLanguage::kChinese;
  UiTheme ui_theme = UiTheme::kMinimal;
  IndicatorMode mode = IndicatorMode::kRing;
  IndicatorStyle ring;
  IndicatorStyle highlight{RGB(255, 255, 0), 24, 1, 25};
  IndicatorStyle custom{RGB(255, 255, 255), 48, 1, 100};
  // Only the library maps a selected name to its managed file.
  std::wstring style_name;
  std::vector<UserStyle> user_styles;
  // Draft positions are independent of immediately saved library maintenance.
  std::map<std::wstring, ImageOffset> image_offsets;
  std::shared_ptr<const ImageAsset> image_asset;
  UINT hotkey_modifiers = 0;
  UINT hotkey_vk = VK_OEM_COMMA;
  UINT hotkey_secondary_vk = VK_OEM_PERIOD;
  int hold_seconds = 3;

  void ResetMissingStyle() {
    mode = IndicatorMode::kRing;
    ring = IndicatorStyle{};
    custom = IndicatorStyle{RGB(255, 255, 255), 48, 1, 100};
    hold_seconds = 3;
    style_name.clear();
    image_asset.reset();
  }

  const IndicatorStyle& active_style() const {
    if (is_custom()) return custom;
    return mode == IndicatorMode::kRing ? ring : highlight;
  }
  bool is_custom() const {
    return mode == IndicatorMode::kImage || mode == IndicatorMode::kGif;
  }

  ImageOffset active_image_offset() const {
    const auto found = image_offsets.find(style_name);
    return is_custom() && found != image_offsets.end() ? found->second
                                                       : ImageOffset{};
  }

  void Normalize() {
    for (auto& entry : image_offsets) {
      entry.second.x_dip = std::clamp(entry.second.x_dip, -kMaxImageOffsetDip,
                                      kMaxImageOffsetDip);
      entry.second.y_dip = std::clamp(entry.second.y_dip, -kMaxImageOffsetDip,
                                      kMaxImageOffsetDip);
    }
    if (static_cast<uint32_t>(ui_language) >
        static_cast<uint32_t>(UiLanguage::kEnglish)) {
      ui_language = UiLanguage::kChinese;
    }
    if (static_cast<uint32_t>(ui_theme) >
        static_cast<uint32_t>(UiTheme::kMinimal)) {
      ui_theme = UiTheme::kMinimal;
    }
    hold_seconds = std::clamp(hold_seconds, 1, 3600);
    if (static_cast<uint32_t>(mode) >
        static_cast<uint32_t>(IndicatorMode::kGif)) {
      mode = IndicatorMode::kRing;
    }
    custom.radius_dip = std::clamp(custom.radius_dip, 8, 256);
    custom.opacity_percent = std::clamp(custom.opacity_percent, 10, 100);
    custom.thickness_dip = 1;
    for (auto* style : {&ring, &highlight}) {
      style->color &= 0x00ffffff;
      style->radius_dip = std::clamp(style->radius_dip, 8, 100);
      style->thickness_dip = std::clamp(
          style->thickness_dip, 1, std::min(20, style->radius_dip * 2 - 1));
      const int min_opacity = style == &highlight ? 0 : 10;
      style->opacity_percent =
          std::clamp(style->opacity_percent, min_opacity, 100);
    }
    if (hotkey_vk == 0 || hotkey_vk > 0xfe ||
        (hotkey_modifiers & ~(MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN)) !=
            0) {
      hotkey_vk = VK_OEM_COMMA;
      hotkey_secondary_vk = VK_OEM_PERIOD;
      hotkey_modifiers = 0;
    }
    if (hotkey_secondary_vk > 0xfe || hotkey_secondary_vk == hotkey_vk) {
      hotkey_secondary_vk = 0;
    }
  }
};

}  // namespace mouse_cursor_detector

#endif  // MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_MODEL_H_
