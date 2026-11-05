#include "mouse_cursor_detector/core/startup_manager.h"

#include <vector>

namespace mouse_cursor_detector {
namespace {
constexpr wchar_t kRunKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"MouseCursorDetector";

std::wstring StartupCommand() {
  const auto path = ExecutablePath();
  return path.empty() ? L"" : L"\"" + path + L"\" --autostart";
}
}  // namespace

std::wstring ExecutablePath() {
  std::vector<wchar_t> buffer(32768);
  const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                          static_cast<DWORD>(buffer.size()));
  if (length == 0 || length >= buffer.size()) return L"";
  return std::wstring(buffer.data(), length);
}

StartupState QueryStartup() {
  StartupState state;
  std::vector<wchar_t> buffer(32768);
  DWORD bytes = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));
  state.error = RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue,
                             RRF_RT_REG_SZ, nullptr, buffer.data(), &bytes);
  if (state.error == ERROR_FILE_NOT_FOUND ||
      state.error == ERROR_PATH_NOT_FOUND) {
    state.error = ERROR_SUCCESS;
    return state;
  }
  if (state.error != ERROR_SUCCESS) return state;
  state.registered = true;
  const auto command = StartupCommand();
  state.current_path = !command.empty() && command == buffer.data();
  return state;
}

LSTATUS SetStartupEnabled(bool enabled) {
  const auto command = StartupCommand();
  if (enabled && command.empty()) return ERROR_BAD_PATHNAME;
  HKEY key = nullptr;
  LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                                   KEY_SET_VALUE, nullptr, &key, nullptr);
  if (status != ERROR_SUCCESS) return status;
  if (enabled) {
    status = RegSetValueExW(
        key, kRunValue, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()),
        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
  } else {
    status = RegDeleteValueW(key, kRunValue);
    if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
  }
  RegCloseKey(key);
  return status;
}
}  // namespace mouse_cursor_detector
