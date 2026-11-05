#include "mouse_cursor_detector/core/hook_manager.h"

#include <shellscalingapi.h>

#include <system_error>

#include "mouse_cursor_detector/interop/window_messages.h"

namespace mouse_cursor_detector {
namespace {
thread_local HookManager* thread_manager = nullptr;
constexpr UINT_PTR kMaintenanceTimer = 1;
constexpr UINT_PTR kAnimationTimer = 2;
}  // namespace

bool HookManager::Start(HWND notification_window) {
  if (thread_.joinable()) return false;
  notification_window_ = notification_window;
  stop_requested_ = false;
  ready_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!ready_) return false;
  try {
    thread_ = std::thread(&HookManager::ThreadMain, this);
  } catch (const std::system_error&) {
    CloseHandle(ready_);
    ready_ = nullptr;
    return false;
  }
  const DWORD result = WaitForSingleObject(ready_, 5000);
  bool initialized;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    initialized = command_window_ != nullptr;
  }
  if (result != WAIT_OBJECT_0 || !initialized) {
    Stop();
    return false;
  }
  return true;
}

void HookManager::Stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_requested_ = true;
    if (command_window_) PostMessageW(command_window_, WM_CLOSE, 0, 0);
  }
  if (thread_.joinable()) thread_.join();
  if (ready_) CloseHandle(ready_);
  ready_ = nullptr;
}

bool HookManager::ApplyState(const Settings& settings, bool visible,
                             bool restart) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!command_window_ || stop_requested_) return false;
  pending_settings_ = settings;
  pending_visible_ = visible;
  pending_restart_ = visible && (pending_restart_ || restart);
  if (!command_pending_) {
    if (!PostMessageW(command_window_, kApplyState, 0, 0)) return false;
    command_pending_ = true;
  }
  return true;
}

void HookManager::ThreadMain() {
  thread_manager = this;
  NativeCursorWindow overlay;
  overlay_ = &overlay;
  WNDCLASSW window_class{};
  window_class.lpfnWndProc = CommandProc;
  window_class.hInstance = GetModuleHandleW(nullptr);
  window_class.lpszClassName = L"MouseCursorDetector.Worker.v1";
  const bool registered = RegisterClassW(&window_class) != 0 ||
                          GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  HWND window = nullptr;
  if (registered && overlay.Create()) {
    window =
        CreateWindowExW(0, window_class.lpszClassName, L"", 0, 0, 0, 0, 0,
                        HWND_MESSAGE, nullptr, window_class.hInstance, this);
  }
  bool stopping;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    command_window_ = window;
    stopping = stop_requested_;
  }
  SetEvent(ready_);
  if (window) {
    MSG message{};
    int result = 0;
    while (!stopping && (result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    if (result == -1) Fail(GetLastError());
    Disable();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      command_window_ = nullptr;
      command_pending_ = false;
    }
    if (IsWindow(window)) DestroyWindow(window);
  }
  overlay_ = nullptr;
  thread_manager = nullptr;
}

LRESULT CALLBACK HookManager::CommandProc(HWND window, UINT message,
                                          WPARAM wparam, LPARAM lparam) {
  auto* self =
      reinterpret_cast<HookManager*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = static_cast<HookManager*>(create->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  if (self) {
    switch (message) {
      case kApplyState:
        self->ApplyPendingState();
        return 0;
      case kRefreshDpi:
        self->dpi_pending_ = false;
        if (self->visible_) self->RefreshDpi();
        return 0;
      case WM_TIMER:
        if (wparam == kMaintenanceTimer) self->Maintain();
        if (wparam == kAnimationTimer) self->Animate();
        return 0;
      case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    }
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

bool HookManager::InstallHook() {
  mouse_hook_ =
      SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(nullptr), 0);
  last_event_ms_ = GetTickCount64();
  return mouse_hook_ != nullptr;
}

void HookManager::ApplyPendingState() {
  bool requested;
  bool restart;
  const auto previous_asset = settings_.image_asset;
  const auto previous_mode = settings_.mode;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_ = pending_settings_;
    requested = pending_visible_;
    restart = pending_restart_;
    pending_restart_ = false;
    command_pending_ = false;
  }
  settings_.Normalize();
  if (!requested) {
    Disable();
  } else {
    if (!visible_ || restart || previous_asset != settings_.image_asset ||
        previous_mode != settings_.mode) {
      frame_index_ = 0;
      animation_start_ms_ = GetTickCount64();
    }
    if (!mouse_hook_ && !InstallHook()) {
      Fail(GetLastError());
      return;
    }
    visible_ = true;
    dpi_ = 0;
    RefreshDpi();
    if (!visible_) return;
    if (!SetTimer(command_window_, kMaintenanceTimer, 400, nullptr)) {
      Fail(GetLastError());
      return;
    }
    KillTimer(command_window_, kAnimationTimer);
    if (settings_.mode == IndicatorMode::kGif && settings_.image_asset &&
        settings_.image_asset->frames.size() > 1 &&
        !SetTimer(command_window_, kAnimationTimer, 20, nullptr)) {
      Fail(GetLastError());
      return;
    }
    if (!foreground_hook_) {
      foreground_hook_ =
          SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                          nullptr, ForegroundProc, 0, 0,
                          WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }
    overlay_->SetVisible(true);
  }
  PostMessageW(notification_window_, kWorkerState, visible_, 0);
}

void HookManager::RefreshDpi() {
  POINT position{};
  if (!GetCursorPos(&position)) {
    Fail(GetLastError());
    return;
  }
  const HMONITOR monitor = MonitorFromPoint(position, MONITOR_DEFAULTTONEAREST);
  UINT dpi_x = 96;
  UINT dpi_y = 96;
  if (FAILED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y))) {
    dpi_x = 96;
  }
  if (dpi_ != dpi_x) {
    if (!overlay_->ApplyStyle(settings_, dpi_x, frame_index_)) {
      Fail(ERROR_GEN_FAILURE);
      return;
    }
    dpi_ = dpi_x;
  }
  overlay_->FollowCursor(position);
}

LRESULT CALLBACK HookManager::MouseProc(int code, WPARAM wparam,
                                        LPARAM lparam) {
  auto* self = thread_manager;
  if (code == HC_ACTION && self && self->visible_) {
    const auto* input = reinterpret_cast<MSLLHOOKSTRUCT*>(lparam);
    self->last_event_ms_ = GetTickCount64();
    self->last_position_ = input->pt;
    if (wparam == WM_MOUSEMOVE) {
      self->overlay_->FollowCursor(input->pt);
      if (!self->dpi_pending_) {
        self->dpi_pending_ =
            PostMessageW(self->command_window_, kRefreshDpi, 0, 0) != 0;
      }
    }
  }
  return CallNextHookEx(nullptr, code, wparam, lparam);
}

void CALLBACK HookManager::ForegroundProc(HWINEVENTHOOK, DWORD, HWND, LONG,
                                          LONG, DWORD, DWORD) {
  if (thread_manager && thread_manager->visible_) {
    thread_manager->overlay_->ReassertTopmost();
  }
}

void HookManager::Maintain() {
  if (!visible_) return;
  overlay_->ReassertTopmost();
  POINT position{};
  if (!GetCursorPos(&position)) return;
  const auto now = GetTickCount64();
  if ((position.x != last_position_.x || position.y != last_position_.y) &&
      now - last_event_ms_ > 1000 && now - last_reinstall_ms_ > 2000) {
    last_reinstall_ms_ = now;
    UnhookWindowsHookEx(mouse_hook_);
    mouse_hook_ = nullptr;
    if (!InstallHook()) {
      Fail(GetLastError());
      return;
    }
  }
  RefreshDpi();
}

void HookManager::Disable() {
  visible_ = false;
  if (overlay_) overlay_->SetVisible(false);
  if (mouse_hook_) UnhookWindowsHookEx(mouse_hook_);
  mouse_hook_ = nullptr;
  if (foreground_hook_) UnhookWinEvent(foreground_hook_);
  foreground_hook_ = nullptr;
  if (command_window_) KillTimer(command_window_, kMaintenanceTimer);
  if (command_window_) KillTimer(command_window_, kAnimationTimer);
}

void HookManager::Animate() {
  if (!visible_ || settings_.mode != IndicatorMode::kGif ||
      !settings_.image_asset)
    return;
  const auto& asset = *settings_.image_asset;
  if (!asset.duration_ms || asset.frames.size() < 2) return;
  uint64_t elapsed =
      (GetTickCount64() - animation_start_ms_) % asset.duration_ms;
  size_t index = 0;
  while (index + 1 < asset.frames.size() &&
         elapsed >= asset.frames[index].delay_ms) {
    elapsed -= asset.frames[index++].delay_ms;
  }
  if (index == frame_index_) return;
  frame_index_ = index;
  if (!overlay_->ApplyStyle(settings_, dpi_, index)) Fail(ERROR_GEN_FAILURE);
}

void HookManager::Fail(DWORD error) {
  Disable();
  PostMessageW(notification_window_, kWorkerError,
               error ? error : ERROR_GEN_FAILURE, 0);
}
}  // namespace mouse_cursor_detector
