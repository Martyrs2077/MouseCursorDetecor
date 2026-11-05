#include "mouse_cursor_detector/settings/settings_store.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <utility>
#include <variant>

namespace mouse_cursor_detector {
// Legacy registry records are consulted only when no portable config exists.
SettingsLoadResult LoadLegacySettings();
namespace {
using Value = std::variant<uint32_t, std::wstring>;
using Object = std::map<std::wstring, Value>;
constexpr size_t kMaxConfigBytes = 2 * 1024 * 1024;

// The portable schema is a flat JSON object of unsigned integers and strings.
class ConfigReader {
 public:
  explicit ConfigReader(const std::wstring& text) : text_(text) {}
  bool Read(Object* object) {
    if (!Take(L'{')) return false;
    if (Take(L'}')) return End();
    for (;;) {
      std::wstring key;
      if (!String(&key) || !Take(L':') || object->count(key)) return false;
      Space();
      if (position_ < text_.size() && text_[position_] == L'"') {
        std::wstring value;
        if (!String(&value)) return false;
        object->emplace(std::move(key), std::move(value));
      } else {
        uint64_t number = 0;
        const size_t begin = position_;
        while (position_ < text_.size() && text_[position_] >= L'0' &&
               text_[position_] <= L'9') {
          number = number * 10 + (text_[position_++] - L'0');
          if (number > std::numeric_limits<uint32_t>::max()) return false;
        }
        if (begin == position_ ||
            (position_ - begin > 1 && text_[begin] == L'0'))
          return false;
        object->emplace(std::move(key), static_cast<uint32_t>(number));
      }
      if (Take(L'}')) return End();
      if (!Take(L',')) return false;
    }
  }

 private:
  void Space() {
    while (position_ < text_.size() &&
           (text_[position_] == L' ' || text_[position_] == L'\r' ||
            text_[position_] == L'\n' || text_[position_] == L'\t'))
      ++position_;
  }
  bool End() {
    Space();
    return position_ == text_.size();
  }
  bool Take(wchar_t c) {
    Space();
    if (position_ == text_.size() || text_[position_] != c) return false;
    ++position_;
    return true;
  }
  bool String(std::wstring* output) {
    if (!Take(L'"')) return false;
    while (position_ < text_.size()) {
      wchar_t c = text_[position_++];
      if (c == L'"') return true;
      if (c < 32) return false;
      if (c == L'\\') {
        if (position_ == text_.size()) return false;
        c = text_[position_++];
        switch (c) {
          case L'"':
          case L'\\':
          case L'/':
            break;
          case L'b':
            c = L'\b';
            break;
          case L'f':
            c = L'\f';
            break;
          case L'n':
            c = L'\n';
            break;
          case L'r':
            c = L'\r';
            break;
          case L't':
            c = L'\t';
            break;
          case L'u': {
            unsigned value = 0;
            for (int i = 0; i < 4; ++i) {
              if (position_ == text_.size()) return false;
              const wchar_t digit = text_[position_++];
              unsigned hex;
              if (digit >= L'0' && digit <= L'9')
                hex = digit - L'0';
              else if (digit >= L'a' && digit <= L'f')
                hex = digit - L'a' + 10;
              else if (digit >= L'A' && digit <= L'F')
                hex = digit - L'A' + 10;
              else
                return false;
              value = value * 16 + hex;
            }
            c = static_cast<wchar_t>(value);
            break;
          }
          default:
            return false;
        }
      }
      output->push_back(c);
    }
    return false;
  }
  const std::wstring& text_;
  size_t position_ = 0;
};

uint32_t Number(const Object& object, const wchar_t* key, uint32_t fallback) {
  const auto it = object.find(key);
  return it != object.end() && std::holds_alternative<uint32_t>(it->second)
             ? std::get<uint32_t>(it->second)
             : fallback;
}

std::wstring StringValue(const Object& object, const std::wstring& key) {
  const auto it = object.find(key);
  return it != object.end() && std::holds_alternative<std::wstring>(it->second)
             ? std::get<std::wstring>(it->second)
             : std::wstring();
}

bool ReadLibrary(const Object& object, std::vector<UserStyle>* styles) {
  const auto count = Number(object, L"style_count", 0);
  if (count > kMaxUserStyles) return false;
  for (uint32_t i = 0; i < count; ++i) {
    const auto prefix = L"style_" + std::to_wstring(i);
    UserStyle entry{StringValue(object, prefix + L"_name"),
                    StringValue(object, prefix + L"_file")};
    if (!IsValidStyleName(entry.name) || HasStyleName(*styles, entry.name) ||
        !IsManagedStylePath(entry.relative_path))
      return false;
    styles->push_back(std::move(entry));
  }
  return true;
}

bool ValidLibrary(const Settings& settings) {
  if (settings.user_styles.size() > kMaxUserStyles) return false;
  std::vector<UserStyle> previous;
  bool selected_found = settings.style_file.empty();
  for (const auto& style : settings.user_styles) {
    if (!IsValidStyleName(style.name) || HasStyleName(previous, style.name) ||
        !IsManagedStylePath(style.relative_path))
      return false;
    previous.push_back(style);
    if (style.relative_path == settings.style_file) selected_found = true;
  }
  return selected_found;
}

std::wstring Quote(const std::wstring& text) {
  std::wstring result = L"\"";
  constexpr wchar_t hex[] = L"0123456789abcdef";
  for (const wchar_t c : text) {
    if (c == L'\\' || c == L'"') {
      result += L'\\';
      result += c;
    } else if (c < 32) {
      result += L"\\u00";
      result += hex[(c >> 4) & 15];
      result += hex[c & 15];
    } else
      result += c;
  }
  return result + L'"';
}

bool Utf8(const std::wstring& text, std::string* output) {
  const int size = WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
      nullptr, 0, nullptr, nullptr);
  if (!size) return false;
  output->resize(size);
  return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                             static_cast<int>(text.size()), output->data(),
                             size, nullptr, nullptr) == size;
}

std::filesystem::path ConfigPath() {
  return std::filesystem::path(ApplicationDirectory()) / L"config" /
         L"settings.json";
}
}  // namespace

SettingsLoadResult LoadSettings() {
  SettingsLoadResult result;
  try {
    if (ApplicationDirectory().empty()) {
      result.invalid = true;
      return result;
    }
    const auto path = ConfigPath();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) && !ec) {
      result = LoadLegacySettings();
      result.needs_save = !result.invalid;
      return result;
    }
    result.first_run = false;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size == 0 || size > kMaxConfigBytes) {
      result.invalid = true;
      return result;
    }
    std::ifstream input(path, std::ios::binary);
    std::string bytes(static_cast<size_t>(size), '\0');
    if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
      result.invalid = true;
      return result;
    }
    if (bytes.compare(0, 3, "\xef\xbb\xbf") == 0) bytes.erase(0, 3);
    const int length =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
                            static_cast<int>(bytes.size()), nullptr, 0);
    std::wstring text(length > 0 ? length : 0, L'\0');
    Object object;
    if (length <= 0 ||
        !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
                             static_cast<int>(bytes.size()), text.data(),
                             length) ||
        !ConfigReader(text).Read(&object) ||
        (Number(object, L"version", 0) != 1 &&
         Number(object, L"version", 0) != 2)) {
      result.invalid = true;
      return result;
    }
    auto& settings = result.settings;
    settings.ui_language =
        static_cast<UiLanguage>(Number(object, L"ui_language", 0));
    settings.ui_theme = static_cast<UiTheme>(Number(object, L"ui_theme", 3));
    settings.mode = static_cast<IndicatorMode>(Number(object, L"mode", 1));
    const auto read_style = [&](const wchar_t* prefix, IndicatorStyle* style) {
      const std::wstring base = prefix;
      style->color = Number(object, (base + L"_color").c_str(), style->color);
      style->radius_dip = static_cast<int>(
          Number(object, (base + L"_radius").c_str(), style->radius_dip));
      style->thickness_dip = static_cast<int>(
          Number(object, (base + L"_thickness").c_str(), style->thickness_dip));
      style->opacity_percent = static_cast<int>(
          Number(object, (base + L"_opacity").c_str(), style->opacity_percent));
    };
    read_style(L"ring", &settings.ring);
    read_style(L"highlight", &settings.highlight);
    read_style(L"custom", &settings.custom);
    settings.hold_seconds =
        static_cast<int>(Number(object, L"hold_seconds", 3));
    settings.hotkey_modifiers = Number(object, L"hotkey_modifiers", 0);
    settings.hotkey_vk = Number(object, L"hotkey_vk", VK_OEM_COMMA);
    settings.hotkey_secondary_vk =
        Number(object, L"hotkey_secondary_vk", VK_OEM_PERIOD);
    const auto file = object.find(L"style_file");
    if (file != object.end() &&
        std::holds_alternative<std::wstring>(file->second)) {
      settings.style_file = std::get<std::wstring>(file->second);
    }
    if (!ReadLibrary(object, &settings.user_styles)) {
      result = {};
      result.invalid = true;
      return result;
    }
    settings.Normalize();
    if (!settings.style_file.empty()) {
      if (!IsManagedStylePath(settings.style_file)) {
        result.style_error = L"配置中的素材路径无效，已回退到内置光圈。";
        settings.style_file.clear();
      } else {
        const bool listed = std::any_of(
            settings.user_styles.begin(), settings.user_styles.end(),
            [&](const UserStyle& style) {
              return style.relative_path == settings.style_file;
            });
        if (!listed) {
          // Version 1 stored only one selected file and no original name.
          auto name =
              std::filesystem::path(settings.style_file).filename().wstring();
          if (Number(object, L"version", 0) != 1 ||
              settings.user_styles.size() >= kMaxUserStyles ||
              HasStyleName(settings.user_styles, name) ||
              !IsValidStyleName(name)) {
            result = {};
            result.invalid = true;
            return result;
          }
          settings.user_styles.push_back(
              {std::move(name), settings.style_file});
          result.needs_save = true;
        }
        const auto full =
            std::filesystem::path(ApplicationDirectory()) / settings.style_file;
        settings.image_asset =
            LoadStyleFile(full.wstring(), &result.style_error);
      }
    }
    if (settings.is_custom() &&
        (!settings.image_asset || (settings.mode == IndicatorMode::kGif &&
                                   !settings.image_asset->is_gif))) {
      if (result.style_error.empty())
        result.style_error = L"所选素材不可用，已回退到内置光圈。";
      settings.mode = IndicatorMode::kRing;
    }
  } catch (const std::exception&) {
    result = {};
    result.invalid = true;
  }
  return result;
}

LSTATUS SaveSettings(const Settings& settings) {
  try {
    Settings checked = settings;
    checked.Normalize();
    if ((!checked.style_file.empty() &&
         !IsManagedStylePath(checked.style_file)) ||
        (checked.is_custom() &&
         (!checked.image_asset || checked.style_file.empty())) ||
        !ValidLibrary(checked)) {
      return ERROR_INVALID_DATA;
    }
    std::wostringstream text;
    text << L"{\n  \"version\": 2,\n  \"ui_language\": "
         << static_cast<uint32_t>(checked.ui_language) << L",\n  \"ui_theme\": "
         << static_cast<uint32_t>(checked.ui_theme) << L",\n  \"mode\": "
         << static_cast<uint32_t>(checked.mode);
    const auto write_style = [&](const wchar_t* name,
                                 const IndicatorStyle& style) {
      text << L",\n  \"" << name << L"_color\": " << style.color << L",\n  \""
           << name << L"_radius\": " << style.radius_dip << L",\n  \"" << name
           << L"_thickness\": " << style.thickness_dip << L",\n  \"" << name
           << L"_opacity\": " << style.opacity_percent;
    };
    write_style(L"ring", checked.ring);
    write_style(L"highlight", checked.highlight);
    write_style(L"custom", checked.custom);
    text << L",\n  \"hold_seconds\": " << checked.hold_seconds
         << L",\n  \"hotkey_modifiers\": " << checked.hotkey_modifiers
         << L",\n  \"hotkey_vk\": " << checked.hotkey_vk
         << L",\n  \"hotkey_secondary_vk\": " << checked.hotkey_secondary_vk
         << L",\n  \"style_file\": " << Quote(checked.style_file)
         << L",\n  \"style_count\": " << checked.user_styles.size();
    for (size_t i = 0; i < checked.user_styles.size(); ++i) {
      const auto prefix = L"style_" + std::to_wstring(i);
      const auto& entry = checked.user_styles[i];
      text << L",\n  " << Quote(prefix + L"_name") << L": " << Quote(entry.name)
           << L",\n  " << Quote(prefix + L"_file") << L": "
           << Quote(entry.relative_path);
    }
    text << L"\n}\n";
    std::string bytes;
    if (!Utf8(text.str(), &bytes)) return ERROR_NO_UNICODE_TRANSLATION;
    if (bytes.size() > kMaxConfigBytes) return ERROR_FILE_TOO_LARGE;
    if (ApplicationDirectory().empty()) return ERROR_BAD_PATHNAME;
    const auto path = ConfigPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return static_cast<LSTATUS>(ec.value());
    const auto temporary = path.wstring() + L".tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return GetLastError();
    DWORD written = 0;
    LSTATUS status = ERROR_SUCCESS;
    if (!WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
                   &written, nullptr) ||
        written != bytes.size() || !FlushFileBuffers(file)) {
      status = GetLastError();
      if (status == ERROR_SUCCESS) status = ERROR_WRITE_FAULT;
    }
    CloseHandle(file);
    if (status == ERROR_SUCCESS &&
        !MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      status = GetLastError();
    if (status != ERROR_SUCCESS) DeleteFileW(temporary.c_str());
    return status;
  } catch (const std::exception&) {
    return ERROR_NOT_ENOUGH_MEMORY;
  }
}
}  // namespace mouse_cursor_detector
