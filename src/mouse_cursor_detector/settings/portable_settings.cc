#include "mouse_cursor_detector/settings/settings_store.h"

#include <cwctype>
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

bool ReadImageOffsets(const Object& object, Settings* settings) {
  const auto count = Number(object, L"image_offset_count", 0);
  const auto field = object.find(L"image_offset_count");
  if ((field != object.end() &&
       !std::holds_alternative<uint32_t>(field->second)) ||
      count > kMaxUserStyles)
    return false;
  for (uint32_t i = 0; i < count; ++i) {
    const auto prefix = L"image_offset_" + std::to_wstring(i);
    const auto name = StringValue(object, prefix + L"_name");
    // Bias signed DIP coordinates to preserve the unsigned flat JSON schema.
    const auto x = Number(object, (prefix + L"_x").c_str(), UINT32_MAX);
    const auto y = Number(object, (prefix + L"_y").c_str(), UINT32_MAX);
    if (!IsValidStyleName(name) || settings->image_offsets.count(name) ||
        x > 2 * kMaxImageOffsetDip || y > 2 * kMaxImageOffsetDip)
      return false;
    settings->image_offsets.emplace(
        name, ImageOffset{static_cast<int>(x) - kMaxImageOffsetDip,
                          static_cast<int>(y) - kMaxImageOffsetDip});
  }
  return true;
}

bool ValidLibrary(const Settings& settings) {
  if (settings.user_styles.size() > kMaxUserStyles) return false;
  std::vector<UserStyle> previous;
  bool selected_found = settings.style_name.empty();
  for (const auto& style : settings.user_styles) {
    if (!IsValidStyleName(style.name) || HasStyleName(previous, style.name) ||
        !IsManagedStylePath(style.relative_path))
      return false;
    previous.push_back(style);
    if (style.name == settings.style_name) selected_found = true;
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

std::filesystem::path LibraryPath() {
  return std::filesystem::path(ApplicationDirectory()) / L"styles" /
         L"library.json";
}

bool SameFile(const std::wstring& left, const std::wstring& right) {
  return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) ==
         CSTR_EQUAL;
}

bool HasFile(const std::vector<UserStyle>& styles, const std::wstring& path) {
  return std::any_of(styles.begin(), styles.end(), [&](const UserStyle& entry) {
    return SameFile(entry.relative_path, path);
  });
}

LSTATUS ReadObjectFile(const std::filesystem::path& path, Object* object,
                       bool* missing) {
  *missing = false;
  const DWORD attributes = GetFileAttributesW(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    const DWORD status = GetLastError();
    if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) {
      *missing = true;
      return ERROR_SUCCESS;
    }
    return status;
  }
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || !size || size > kMaxConfigBytes) return ERROR_INVALID_DATA;
  std::ifstream input(path, std::ios::binary);
  std::string bytes(static_cast<size_t>(size), '\0');
  if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size())))
    return ERROR_READ_FAULT;
  if (bytes.compare(0, 3, "\xef\xbb\xbf") == 0) bytes.erase(0, 3);
  const int length =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
                          static_cast<int>(bytes.size()), nullptr, 0);
  std::wstring text(length > 0 ? length : 0, L'\0');
  if (length <= 0 ||
      !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
                           static_cast<int>(bytes.size()), text.data(),
                           length) ||
      !ConfigReader(text).Read(object))
    return ERROR_INVALID_DATA;
  return ERROR_SUCCESS;
}

LSTATUS WriteTextFile(const std::filesystem::path& path,
                      const std::wstring& text) {
  if (ApplicationDirectory().empty()) return ERROR_BAD_PATHNAME;
  std::string bytes;
  if (!Utf8(text, &bytes)) return ERROR_NO_UNICODE_TRANSLATION;
  if (bytes.size() > kMaxConfigBytes) return ERROR_FILE_TOO_LARGE;
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) return static_cast<LSTATUS>(ec.value());
  const auto temporary = path.wstring() + L".tmp";
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return GetLastError();
  DWORD written = 0;
  LSTATUS status = ERROR_SUCCESS;
  if (!WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written,
                 nullptr) ||
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
}

std::wstring UniqueName(const std::vector<UserStyle>& styles,
                        const std::wstring& filename) {
  const std::filesystem::path path(filename);
  std::wstring name = filename;
  for (uint32_t suffix = 1; HasStyleName(styles, name); ++suffix)
    name = path.stem().wstring() + L"_" + std::to_wstring(suffix) +
           path.extension().wstring();
  return name;
}
}  // namespace

bool LoadStyleLibrary(std::vector<UserStyle>* styles, std::wstring* error) {
  try {
    error->clear();
    if (ApplicationDirectory().empty()) {
      *error = L"无法确定软件目录。";
      return false;
    }
    Object object;
    bool missing = false;
    const auto status = ReadObjectFile(LibraryPath(), &object, &missing);
    if (status == ERROR_SUCCESS && missing) return true;
    std::vector<UserStyle> loaded;
    if (status != ERROR_SUCCESS || Number(object, L"version", 0) != 1 ||
        !object.count(L"style_count") ||
        !std::holds_alternative<uint32_t>(object.at(L"style_count")) ||
        !ReadLibrary(object, &loaded)) {
      *error = L"素材映射表无法读取或已损坏，未覆盖原文件。";
      return false;
    }
    std::vector<UserStyle> previous;
    for (const auto& entry : loaded) {
      if (HasFile(previous, entry.relative_path)) {
        *error = L"素材映射表包含重复文件，请检查 library.json。";
        return false;
      }
      previous.push_back(entry);
    }
    *styles = std::move(loaded);
    return true;
  } catch (const std::exception&) {
    *error = L"素材映射表无法读取或已损坏，未覆盖原文件。";
    return false;
  }
}

LSTATUS SaveStyleLibrary(const std::vector<UserStyle>& styles) {
  try {
    Settings checked;
    checked.user_styles = styles;
    if (!ValidLibrary(checked)) return ERROR_INVALID_DATA;
    std::vector<UserStyle> existing;
    std::wstring error;
    if (!LoadStyleLibrary(&existing, &error)) return ERROR_INVALID_DATA;
    std::wostringstream text;
    text << L"{\n  \"version\": 1,\n  \"style_count\": " << styles.size();
    std::vector<UserStyle> previous;
    for (size_t i = 0; i < styles.size(); ++i) {
      const auto& entry = styles[i];
      if (HasFile(previous, entry.relative_path)) return ERROR_INVALID_DATA;
      previous.push_back(entry);
      const auto prefix = L"style_" + std::to_wstring(i);
      text << L",\n  " << Quote(prefix + L"_name") << L": " << Quote(entry.name)
           << L",\n  " << Quote(prefix + L"_file") << L": "
           << Quote(entry.relative_path);
    }
    text << L"\n}\n";
    return WriteTextFile(LibraryPath(), text.str());
  } catch (const std::exception&) {
    return ERROR_NOT_ENOUGH_MEMORY;
  }
}

std::shared_ptr<const ImageAsset> LoadMappedStyle(
    const std::vector<UserStyle>& styles, const std::wstring& name,
    std::wstring* error, bool thumbnail) {
  for (const auto& entry : styles) {
    if (entry.name != name) continue;
    if (!IsManagedStylePath(entry.relative_path)) break;
    const auto full =
        std::filesystem::path(ApplicationDirectory()) / entry.relative_path;
    return LoadStyleFile(full.wstring(), error, thumbnail);
  }
  *error = L"素材名称未登记在映射表中。";
  return nullptr;
}

StyleLibraryResult RefreshStyleLibrary(const std::vector<UserStyle>& styles,
                                       const std::vector<UserStyle>& baseline,
                                       const std::wstring& selected_name) {
  StyleLibraryResult result;
  try {
    auto entries = styles;
    if (!LoadStyleLibrary(&entries, &result.error)) return result;
    // Keep unsaved imports, but accept external edits to saved map entries.
    for (const auto& entry : styles) {
      if (HasFile(entries, entry.relative_path) ||
          HasFile(baseline, entry.relative_path))
        continue;
      if (HasStyleName(entries, entry.name)) {
        result.error = L"素材映射名称发生冲突，请先保存或取消当前导入。";
        return result;
      }
      entries.push_back(entry);
    }
    if (entries.size() > kMaxUserStyles) {
      result.error = L"样式库最多保存 512 个素材。";
      return result;
    }
    for (const auto& entry : entries) {
      const auto full =
          std::filesystem::path(ApplicationDirectory()) / entry.relative_path;
      const DWORD attributes = GetFileAttributesW(full.c_str());
      if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD status = GetLastError();
        if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) {
          result.missing_names.push_back(entry.name);
          continue;
        }
      }
      result.styles.push_back(entry);
    }
    const auto directory =
        std::filesystem::path(ApplicationDirectory()) / L"styles";
    std::error_code ec;
    std::vector<std::filesystem::path> discovered;
    std::filesystem::directory_iterator iterator(directory, ec), end;
    if (ec && ec != std::errc::no_such_file_or_directory) {
      result.error = L"无法刷新素材目录，请检查目录读取权限。";
      return result;
    }
    while (!ec && iterator != end) {
      const auto path = iterator->path();
      const auto attributes = GetFileAttributesW(path.c_str());
      if (attributes != INVALID_FILE_ATTRIBUTES &&
          !(attributes &
            (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
        discovered.push_back(path);
      iterator.increment(ec);
    }
    if (ec && ec != std::errc::no_such_file_or_directory) {
      result.error = L"素材目录读取中断，保留当前素材列表。";
      return result;
    }
    std::sort(discovered.begin(), discovered.end());
    for (const auto& path : discovered) {
      const auto relative = L"styles/" + path.filename().wstring();
      auto extension = path.extension().wstring();
      std::transform(extension.begin(), extension.end(), extension.begin(),
                     [](wchar_t c) { return std::towlower(c); });
      constexpr wchar_t supported[] =
          L"|.png|.jpg|.jpeg|.bmp|.tif|.tiff|.ico|.gif|.jxr|.webp|.heic|.heif|."
          L"avif|";
      if (extension.empty() ||
          std::wstring(supported).find(L"|" + extension + L"|") ==
              std::wstring::npos ||
          !IsManagedStylePath(relative) || HasFile(result.styles, relative) ||
          result.styles.size() >= kMaxUserStyles)
        continue;
      const auto name = UniqueName(result.styles, path.filename().wstring());
      if (!IsValidStyleName(name)) continue;
      result.styles.push_back({name, relative});
      std::wstring error;
      if (!LoadMappedStyle(result.styles, name, &error, true))
        result.styles.pop_back();
    }
    for (const auto& entry : result.styles) {
      std::wstring error;
      result.thumbnails.push_back(
          LoadMappedStyle(result.styles, entry.name, &error, true));
    }
    if (!selected_name.empty()) {
      std::wstring error;
      result.selected_asset =
          LoadMappedStyle(result.styles, selected_name, &error);
    }
    // Decoding the library can take time; catch files removed during that work.
    for (size_t i = result.styles.size(); i > 0; --i) {
      const auto& entry = result.styles[i - 1];
      const auto full =
          std::filesystem::path(ApplicationDirectory()) / entry.relative_path;
      if (GetFileAttributesW(full.c_str()) != INVALID_FILE_ATTRIBUTES) continue;
      const DWORD status = GetLastError();
      if (status != ERROR_FILE_NOT_FOUND && status != ERROR_PATH_NOT_FOUND)
        continue;
      result.missing_names.push_back(entry.name);
      if (entry.name == selected_name) result.selected_asset.reset();
      result.styles.erase(result.styles.begin() + (i - 1));
      result.thumbnails.erase(result.thumbnails.begin() + (i - 1));
    }
    if (SaveStyleLibrary(result.styles) != ERROR_SUCCESS)
      result.error = L"素材列表已刷新，但映射表未能保存，请检查写入权限。";
    result.success = true;
  } catch (const std::exception&) {
    result.error = L"素材刷新失败，请检查文件和可用内存。";
  }
  return result;
}

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
      if (!LoadStyleLibrary(&result.settings.user_styles,
                            &result.style_error)) {
        result.invalid = true;
        result.needs_save = false;
      }
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
         Number(object, L"version", 0) != 2 &&
         Number(object, L"version", 0) != 3 &&
         Number(object, L"version", 0) != 4)) {
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
    const auto version = Number(object, L"version", 0);
    if (version >= 4 && !ReadImageOffsets(object, &settings)) {
      result = {};
      result.invalid = true;
      return result;
    }
    std::wstring legacy_file;
    if (version < 3) {
      legacy_file = StringValue(object, L"style_file");
      if (!ReadLibrary(object, &settings.user_styles)) {
        result = {};
        result.invalid = true;
        return result;
      }
      if (!legacy_file.empty() && IsManagedStylePath(legacy_file) &&
          !HasFile(settings.user_styles, legacy_file) && version == 1) {
        const auto name =
            UniqueName(settings.user_styles,
                       std::filesystem::path(legacy_file).filename().wstring());
        if (settings.user_styles.size() < kMaxUserStyles &&
            IsValidStyleName(name))
          settings.user_styles.push_back({name, legacy_file});
      }
      // Preserve legacy files until the user explicitly saves configuration.
      result.needs_save = false;
    } else {
      settings.style_name = StringValue(object, L"style_name");
    }
    settings.Normalize();
    if (!LoadStyleLibrary(&settings.user_styles, &result.style_error)) {
      settings.user_styles.clear();
      if (settings.is_custom()) {
        settings.ResetMissingStyle();
      } else {
        settings.style_name.clear();
        settings.image_asset.reset();
      }
      // Keep a damaged map untouched; don't automatically save defaults over
      // it.
      result.needs_save = false;
      return result;
    }
    if (version < 3) {
      for (const auto& entry : settings.user_styles)
        if (SameFile(entry.relative_path, legacy_file))
          settings.style_name = entry.name;
    }
    settings.Normalize();
    if (!settings.style_name.empty())
      settings.image_asset = LoadMappedStyle(
          settings.user_styles, settings.style_name, &result.style_error);
    if ((settings.is_custom() && !settings.image_asset) ||
        (!settings.style_name.empty() &&
         !HasStyleName(settings.user_styles, settings.style_name))) {
      if (settings.is_custom()) {
        if (result.style_error.empty())
          result.style_error = L"所选素材不可用，已回退到默认光圈及默认参数。";
        settings.ResetMissingStyle();
      } else {
        settings.style_name.clear();
        settings.image_asset.reset();
      }
    } else if (settings.is_custom() && settings.image_asset) {
      settings.mode = settings.image_asset->is_gif ? IndicatorMode::kGif
                                                   : IndicatorMode::kImage;
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
    if ((checked.is_custom() &&
         (!checked.image_asset || checked.style_name.empty())) ||
        !ValidLibrary(checked)) {
      return ERROR_INVALID_DATA;
    }
    std::wostringstream text;
    text << L"{\n  \"version\": 4,\n  \"ui_language\": "
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
         << L",\n  \"style_name\": " << Quote(checked.style_name);
    size_t offset_count = 0;
    for (const auto& entry : checked.user_styles)
      if (checked.image_offsets.count(entry.name)) ++offset_count;
    text << L",\n  \"image_offset_count\": " << offset_count;
    size_t index = 0;
    for (const auto& entry : checked.user_styles) {
      const auto found = checked.image_offsets.find(entry.name);
      if (found == checked.image_offsets.end()) continue;
      const auto prefix = L"image_offset_" + std::to_wstring(index++);
      text << L",\n  " << Quote(prefix + L"_name") << L": " << Quote(entry.name)
           << L",\n  " << Quote(prefix + L"_x") << L": "
           << found->second.x_dip + kMaxImageOffsetDip << L",\n  "
           << Quote(prefix + L"_y") << L": "
           << found->second.y_dip + kMaxImageOffsetDip;
    }
    text << L"\n}\n";
    const auto library_status = SaveStyleLibrary(checked.user_styles);
    if (library_status != ERROR_SUCCESS) return library_status;
    return WriteTextFile(ConfigPath(), text.str());
  } catch (const std::exception&) {
    return ERROR_NOT_ENOUGH_MEMORY;
  }
}
}  // namespace mouse_cursor_detector
