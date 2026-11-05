#include "mouse_cursor_detector/app.h"

#include <commctrl.h>
#include <wtsapi32.h>

#include <string>

#include "mouse_cursor_detector/core/startup_manager.h"
#include "mouse_cursor_detector/interop/window_messages.h"
#include "mouse_cursor_detector/resources/resource.h"
#include "mouse_cursor_detector/settings/settings_store.h"

namespace mouse_cursor_detector {
namespace {
constexpr UINT_PTR kAutoHideTimer = 1;

void ShowAbout(HWND owner) {
  const HMODULE module = GetModuleHandleW(nullptr);
  const HRSRC resource =
      FindResourceW(module, MAKEINTRESOURCEW(2000), RT_RCDATA);
  std::wstring text =
      Tr(L"MouseCursorDetector 0.1.0\n\n"
         L"透明底色 / 空心光圈；不替换、不隐藏系统光标。\n"
         L"默认同时按下逗号键和句号键触发提示，3 秒后关闭；时长可修改。\n\n"
         L"参考 MAXsCursor 的原生窗口与跟随线程设计。\n\n");
  if (resource) {
    const auto* bytes =
        static_cast<const char*>(LockResource(LoadResource(module, resource)));
    const int size = static_cast<int>(SizeofResource(module, resource));
    if (bytes && size > 0) {
      const int count =
          MultiByteToWideChar(CP_UTF8, 0, bytes, size, nullptr, 0);
      const auto start = text.size();
      text.resize(start + count);
      MultiByteToWideChar(CP_UTF8, 0, bytes, size, text.data() + start, count);
    }
  }
  MessageBoxW(owner, text.c_str(), Tr(L"关于 / 开源许可"), MB_OK);
}
}  // namespace

int App::Run(bool autostart) {
  const auto loaded = LoadSettings();
  settings_ = loaded.settings;
  SetUiLanguage(settings_.ui_language);
  displayed_settings_ = settings_;
  const LSTATUS initial_save =
      loaded.needs_save ? SaveSettings(settings_) : ERROR_SUCCESS;
  // Starting the process never enables the visual effect, including autostart.
  requested_visible_ = false;
  taskbar_created_ = RegisterWindowMessageW(L"TaskbarCreated");
  WNDCLASSW window_class{};
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = GetModuleHandleW(nullptr);
  window_class.hIcon =
      LoadIconW(window_class.hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
  window_class.lpszClassName = kControlClass;
  if (!RegisterClassW(&window_class) &&
      GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return 1;
  }
  window_ = CreateWindowExW(WS_EX_TOOLWINDOW, kControlClass,
                            L"MouseCursorDetector", WS_OVERLAPPED, 0, 0, 0, 0,
                            nullptr, nullptr, window_class.hInstance, this);
  if (!window_) return 1;
  if (!tray_.Add(window_)) {
    MessageBoxW(window_, Tr(L"无法创建托盘图标。程序将退出。"), Tr(L"鼠标提示"),
                MB_ICONERROR);
    return 1;
  }
  settings_window_ = std::make_unique<SettingsWindow>(
      [this](const Settings& settings) { Preview(settings); },
      [this](const Settings& settings) { return Commit(settings); },
      [this]() { ToggleStartup(); },
      [this](const Settings& settings) {
        // Deleted assets must stop being used even when saving is unavailable.
        settings_ = settings;
        Preview(settings_);
      });
  if (!hotkey_.Replace(window_, settings_.hotkey_modifiers, settings_.hotkey_vk,
                       settings_.hotkey_secondary_vk)) {
    Report(Tr(L"键盘输入监听初始化失败，请退出后重试。"), GetLastError());
  }
  worker_ready_ = hooks_.Start(window_);
  if (!worker_ready_) {
    Report(Tr(L"提示线程初始化失败。可退出后重试。"), ERROR_NOT_READY);
  } else {
    hooks_.ApplyState(settings_, requested_visible_);
  }
  WTSRegisterSessionNotification(window_, NOTIFY_FOR_THIS_SESSION);
  if (loaded.invalid) {
    tray_.Warn(Tr(L"设置无法读取或版本不受支持，本次使用默认值。"));
  }
  if (!loaded.style_error.empty()) tray_.Warn(Tr(loaded.style_error.c_str()));
  if (initial_save != ERROR_SUCCESS) {
    Report(Tr(L"无法创建 config/settings.json，请检查软件目录写入权限。"),
           initial_save);
  }
  const auto startup = QueryStartup();
  if (startup.registered && !startup.current_path) {
    tray_.Warn(Tr(L"自启动路径与当前位置不同，请在设置中修正。"));
  }
  if (!autostart) ShowSettings();
  MSG message{};
  int result;
  while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
    if (settings_window_ && settings_window_->HandleDialogMessage(&message)) {
      continue;
    }
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return result == -1 ? 1 : 0;
}

App::~App() {
  if (window_) KillTimer(window_, kAutoHideTimer);
  requested_visible_ = false;
  hide_deadline_ms_ = 0;
  settings_window_.reset();
  hotkey_.Reset();
  hooks_.Stop();
  tray_.Remove();
  if (window_) {
    WTSUnRegisterSessionNotification(window_);
    DestroyWindow(window_);
  }
}

void App::Report(const wchar_t* context, DWORD error) {
  wchar_t* description = nullptr;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, error, 0, reinterpret_cast<wchar_t*>(&description), 0,
                 nullptr);
  std::wstring text = context;
  text += Tr(L"\n错误 ") + std::to_wstring(error);
  if (description) {
    text += CurrentUiLanguage() == UiLanguage::kEnglish ? L": " : L"：";
    text += description;
    LocalFree(description);
  }
  const HWND owner = settings_window_ && settings_window_->is_open()
                         ? settings_window_->window()
                         : window_;
  MessageBoxW(owner, text.c_str(), Tr(L"鼠标提示"), MB_OK | MB_ICONWARNING);
}

void App::TriggerIndicator() {
  if (session_locked_ ||
      (settings_window_ && settings_window_->IsCapturingHotkey())) {
    return;
  }
  if (!worker_ready_) {
    Report(Tr(L"提示线程不可用，请退出后重新启动。"), ERROR_NOT_READY);
    return;
  }
  const UINT duration_ms = static_cast<UINT>(settings_.hold_seconds) * 1000;
  if (!SetTimer(window_, kAutoHideTimer, duration_ms, nullptr)) {
    const DWORD error = GetLastError();
    HideIndicator();
    Report(Tr(L"无法设置自动关闭计时器，提示未开启。"), error);
    return;
  }
  hide_deadline_ms_ = GetTickCount64() + duration_ms;
  requested_visible_ = true;
  if (!hooks_.ApplyState(displayed_settings_, true, true)) {
    HideIndicator();
    Report(Tr(L"无法更新提示状态。"), ERROR_NOT_READY);
    return;
  }
}

void App::HideIndicator() {
  KillTimer(window_, kAutoHideTimer);
  hide_deadline_ms_ = 0;
  requested_visible_ = false;
  if (worker_ready_) hooks_.ApplyState(displayed_settings_, false);
}

void App::ExpireIndicatorIfNeeded() {
  // A queued message from an earlier timer must not end a retriggered effect.
  if (requested_visible_ && GetTickCount64() >= hide_deadline_ms_) {
    HideIndicator();
  }
}

void App::Preview(const Settings& settings) {
  ExpireIndicatorIfNeeded();
  const bool language_changed =
      displayed_settings_.ui_language != settings.ui_language;
  displayed_settings_ = settings;
  SetUiLanguage(settings.ui_language);
  if (language_changed) tray_.Update(actual_visible_, settings.mode);
  if (worker_ready_) {
    hooks_.ApplyState(settings, requested_visible_ && !session_locked_);
  }
}

bool App::Commit(const Settings& candidate) {
  if (!hotkey_.Replace(window_, candidate.hotkey_modifiers, candidate.hotkey_vk,
                       candidate.hotkey_secondary_vk)) {
    Report(Tr(L"键盘输入监听初始化失败，设置未保存。"), GetLastError());
    return false;
  }
  const LSTATUS status = SaveSettings(candidate);
  if (status != ERROR_SUCCESS) {
    const bool restored =
        hotkey_.Replace(window_, settings_.hotkey_modifiers,
                        settings_.hotkey_vk, settings_.hotkey_secondary_vk);
    Report(Tr(L"设置未能保存，窗口将保持打开。"), status);
    if (!restored) tray_.Warn(Tr(L"原快捷键恢复失败，请重新选择快捷键。"));
    return false;
  }
  settings_ = candidate;
  Preview(settings_);
  return true;
}

void App::ShowSettings() {
  if (settings_window_) settings_window_->Show(window_, settings_);
}

void App::ToggleStartup() {
  const auto state = QueryStartup();
  if (state.error != ERROR_SUCCESS) {
    Report(Tr(L"无法读取登录自启动设置。"), state.error);
    return;
  }
  // A stale path is repaired rather than silently creating another copy.
  const bool enable = !state.registered || !state.current_path;
  const auto status = SetStartupEnabled(enable);
  if (status != ERROR_SUCCESS) {
    Report(Tr(L"自启动设置未能更新。"), status);
  } else if (enable) {
    tray_.Warn(
        Tr(L"已登记登录自启动。如果 Windows 启动应用中被禁用，请在那里启用。"));
  }
  if (settings_window_) settings_window_->RefreshStartup();
}

LRESULT CALLBACK App::WindowProc(HWND window, UINT message, WPARAM wparam,
                                 LPARAM lparam) {
  auto* self = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    self = static_cast<App*>(
        reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
    self->window_ = window;
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  return self ? self->HandleMessage(message, wparam, lparam)
              : DefWindowProcW(window, message, wparam, lparam);
}

LRESULT App::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
  ExpireIndicatorIfNeeded();
  if (taskbar_created_ && message == taskbar_created_) {
    if (!tray_.Add(window_)) {
      Report(Tr(L"托盘恢复失败，请退出后重试。"), ERROR_GEN_FAILURE);
    }
    tray_.Update(actual_visible_, displayed_settings_.mode);
    return 0;
  }
  switch (message) {
    case kTrayMessage:
      if (lparam == WM_LBUTTONDBLCLK) ShowSettings();
      if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU) {
        tray_.ShowMenu(window_, QueryStartup().registered);
      }
      return 0;
    case WM_INPUT:
      if (hotkey_.ProcessInput(reinterpret_cast<HRAWINPUT>(lparam)) &&
          !session_locked_ &&
          !(settings_window_ && settings_window_->IsCapturingHotkey())) {
        TriggerIndicator();
      }
      // Foreground raw input requires DefWindowProc cleanup.
      return DefWindowProcW(window_, message, wparam, lparam);
    case kOpenSettings:
      ShowSettings();
      return 0;
    case WM_TIMER:
      if (wparam == kAutoHideTimer) {
        ExpireIndicatorIfNeeded();
        return 0;
      }
      break;
    case kWorkerState:
      actual_visible_ = wparam != 0;
      tray_.Update(actual_visible_, displayed_settings_.mode);
      return 0;
    case kWorkerError:
      actual_visible_ = false;
      HideIndicator();
      tray_.Update(false, displayed_settings_.mode);
      Report(Tr(L"提示层已停止；请退出后重新启动程序。"),
             static_cast<DWORD>(wparam));
      return 0;
    case WM_WTSSESSION_CHANGE:
      if (wparam == WTS_SESSION_LOCK || wparam == WTS_SESSION_UNLOCK) {
        session_locked_ = wparam == WTS_SESSION_LOCK;
        hotkey_.RefreshKeyState();
        if (worker_ready_) {
          hooks_.ApplyState(displayed_settings_,
                            requested_visible_ && !session_locked_);
        }
      }
      return 0;
    case WM_POWERBROADCAST:
      if (wparam == PBT_APMRESUMEAUTOMATIC && worker_ready_) {
        hotkey_.RefreshKeyState();
        hooks_.ApplyState(displayed_settings_,
                          requested_visible_ && !session_locked_);
      }
      return TRUE;
    case WM_COMMAND:
      switch (LOWORD(wparam)) {
        case kSettingsCommand:
          ShowSettings();
          break;
        case kStartupCommand:
          ToggleStartup();
          break;
        case kAboutCommand:
          ShowAbout(window_);
          break;
        case kQuitCommand:
          PostQuitMessage(0);
          break;
      }
      return 0;
    case WM_CLOSE:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(window_, message, wparam, lparam);
}
}  // namespace mouse_cursor_detector
