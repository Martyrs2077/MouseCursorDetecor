#include "mouse_cursor_detector/tray/tray_icon.h"

#include <cwchar>

#include "mouse_cursor_detector/interop/window_messages.h"
#include "mouse_cursor_detector/resources/resource.h"

namespace mouse_cursor_detector {
bool TrayIcon::Add(HWND owner) {
  data_.cbSize = sizeof(data_);
  data_.hWnd = owner;
  data_.uID = 1;
  data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  data_.uCallbackMessage = kTrayMessage;
  data_.hIcon = static_cast<HICON>(LoadImageW(
      GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
      GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
  if (!data_.hIcon) return false;
  std::wcscpy(data_.szTip, Tr(L"鼠标提示 · 等待快捷键"));
  added_ = Shell_NotifyIconW(NIM_ADD, &data_) != 0;
  return added_;
}

void TrayIcon::Update(bool enabled, IndicatorMode mode) {
  if (!added_) return;
  const wchar_t* name = Tr(L"光圈");
  if (mode == IndicatorMode::kHighlight) name = Tr(L"高亮");
  if (mode == IndicatorMode::kImage) name = Tr(L"图片");
  if (mode == IndicatorMode::kGif) name = Tr(L"GIF 动画");
  std::swprintf(data_.szTip, sizeof(data_.szTip) / sizeof(wchar_t),
                Tr(L"鼠标提示 · %ls · %ls"), name,
                enabled ? Tr(L"正在显示") : Tr(L"等待快捷键"));
  data_.uFlags = NIF_TIP;
  Shell_NotifyIconW(NIM_MODIFY, &data_);
}

void TrayIcon::Warn(const wchar_t* message) {
  if (!added_) return;
  data_.uFlags = NIF_INFO;
  std::wcscpy(data_.szInfoTitle, Tr(L"鼠标提示"));
  std::wcsncpy(data_.szInfo, message,
               sizeof(data_.szInfo) / sizeof(wchar_t) - 1);
  data_.szInfo[sizeof(data_.szInfo) / sizeof(wchar_t) - 1] = L'\0';
  data_.dwInfoFlags = NIIF_WARNING;
  Shell_NotifyIconW(NIM_MODIFY, &data_);
}

void TrayIcon::ShowMenu(HWND owner, bool startup_enabled) {
  HMENU menu = CreatePopupMenu();
  if (!menu) return;
  AppendMenuW(menu, MF_STRING, kSettingsCommand, Tr(L"打开设置菜单"));
  AppendMenuW(menu, MF_STRING | (startup_enabled ? MF_CHECKED : MF_UNCHECKED),
              kStartupCommand, Tr(L"开机自启动"));
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kAboutCommand, Tr(L"关于"));
  AppendMenuW(menu, MF_STRING, kQuitCommand, Tr(L"退出"));
  POINT position{};
  GetCursorPos(&position);
  SetForegroundWindow(owner);
  const UINT command =
      TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, position.x,
                     position.y, 0, owner, nullptr);
  DestroyMenu(menu);
  if (command) PostMessageW(owner, WM_COMMAND, command, 0);
  PostMessageW(owner, WM_NULL, 0, 0);
}

void TrayIcon::Remove() {
  if (added_) Shell_NotifyIconW(NIM_DELETE, &data_);
  added_ = false;
}

TrayIcon::~TrayIcon() { Remove(); }
}  // namespace mouse_cursor_detector
