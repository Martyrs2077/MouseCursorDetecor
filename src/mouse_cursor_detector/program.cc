#include <windows.h>

#include <objidl.h>

#include <commctrl.h>
#include <gdiplus.h>

#include <cwchar>
#include <exception>

#include "mouse_cursor_detector/app.h"
#include "mouse_cursor_detector/interop/window_messages.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR command_line, int) {
  using mouse_cursor_detector::App;
  using mouse_cursor_detector::Tr;
  HANDLE instance = CreateMutexW(
      nullptr, FALSE, L"Local\\MouseCursorDetector.SingleInstance.v1");
  if (!instance) return 1;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    HWND existing = nullptr;
    for (int attempt = 0; attempt < 20 && !existing; ++attempt) {
      existing = FindWindowW(mouse_cursor_detector::kControlClass, nullptr);
      if (!existing) Sleep(50);
    }
    if (existing) {
      PostMessageW(existing, mouse_cursor_detector::kOpenSettings, 0, 0);
    }
    CloseHandle(instance);
    return 0;
  }
  INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES};
  InitCommonControlsEx(&controls);
  Gdiplus::GdiplusStartupInput input;
  ULONG_PTR graphics_token = 0;
  if (Gdiplus::GdiplusStartup(&graphics_token, &input, nullptr) !=
      Gdiplus::Ok) {
    CloseHandle(instance);
    return 1;
  }
  int exit_code = 1;
  try {
    App app;
    exit_code = app.Run(command_line &&
                        std::wcsstr(command_line, L"--autostart") != nullptr);
  } catch (const std::exception&) {
    MessageBoxW(nullptr, Tr(L"程序初始化失败或内存不足。"), Tr(L"鼠标提示"),
                MB_ICONERROR);
  }
  Gdiplus::GdiplusShutdown(graphics_token);
  CloseHandle(instance);
  return exit_code;
}
