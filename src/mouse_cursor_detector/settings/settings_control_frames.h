#ifndef MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_CONTROL_FRAMES_H_
#define MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_CONTROL_FRAMES_H_

#include "mouse_cursor_detector/settings/settings_ui.h"

namespace mouse_cursor_detector::settings_ui {

inline constexpr wchar_t kFieldProperty[] = L"MouseCursorDetector.Field";
inline constexpr wchar_t kFieldHotProperty[] = L"MouseCursorDetector.FieldHot";
inline constexpr wchar_t kFieldErrorProperty[] = L"MouseCursorDetector.FieldError";
inline constexpr COLORREF kFieldBorder = RGB(229, 231, 235);
inline constexpr COLORREF kFieldFocus = RGB(37, 99, 235);
inline constexpr COLORREF kFieldError = RGB(220, 38, 38);
inline constexpr int kFieldFrameInset = 2;
inline constexpr int kFieldCornerRadius = 8;

inline void SetFieldError(HWND window, bool error) {
  if (!window || (GetPropW(window, kFieldErrorProperty) != nullptr) == error)
    return;
  if (error)
    SetPropW(window, kFieldErrorProperty, reinterpret_cast<HANDLE>(1));
  else
    RemovePropW(window, kFieldErrorProperty);
  RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
}

inline COLORREF FieldFill(HWND window) {
  return IsWindowEnabled(window) ? RGB(255, 255, 255) : RGB(229, 231, 235);
}

inline HBRUSH FieldBrush(HWND window, HDC dc) {
  return ControlBrush(dc, FieldFill(window),
                      IsWindowEnabled(window) ? RGB(31, 41, 55)
                                              : RGB(107, 114, 128));
}

inline void TrackFieldHover(HWND window, UINT message) {
  if ((message == WM_MOUSEMOVE || message == WM_NCMOUSEMOVE) &&
      IsWindowEnabled(window) && !GetPropW(window, kFieldHotProperty)) {
    DWORD flags = TME_LEAVE;
    if (message == WM_NCMOUSEMOVE) flags |= TME_NONCLIENT;
    TRACKMOUSEEVENT tracking{sizeof(tracking), flags, window, 0};
    if (TrackMouseEvent(&tracking)) {
      SetPropW(window, kFieldHotProperty, reinterpret_cast<HANDLE>(1));
      RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
    }
  }
  if (message == WM_MOUSELEAVE || message == WM_NCMOUSELEAVE ||
      message == WM_ENABLE) {
    RemovePropW(window, kFieldHotProperty);
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
  }
}

inline void RemoveFieldProperties(HWND window) {
  RemovePropW(window, kFieldProperty);
  RemovePropW(window, kFieldHotProperty);
  RemovePropW(window, kFieldErrorProperty);
}

inline void DrawFieldSurface(HWND window, HDC dc, RECT bounds,
                             COLORREF background, bool focused) {
  const bool enabled = IsWindowEnabled(window) != FALSE;
  const bool error = GetPropW(window, kFieldErrorProperty) != nullptr;
  const bool hot = GetPropW(window, kFieldHotProperty) != nullptr;
  Fill(dc, bounds, background);
  // The focused surface has one solid blue edge, with no layered halo or
  // additional dotted frame. Keep it inside the existing HWND.
  InflateRect(&bounds, -Scale(window, kFieldFrameInset),
              -Scale(window, kFieldFrameInset));
  const COLORREF border = !enabled  ? RGB(190, 194, 202)
                          : error   ? kFieldError
                          : focused ? kFieldFocus
                          : hot     ? RGB(148, 163, 184)
                                    : kFieldBorder;
  RoundedBox(window, dc, bounds, FieldFill(window), border,
             Scale(window, kFieldCornerRadius));
}

inline void PaintFieldFrame(HWND window, HDC dc, COLORREF background) {
  RECT bounds{}, client{};
  GetWindowRect(window, &bounds);
  const POINT origin{bounds.left, bounds.top};
  OffsetRect(&bounds, -origin.x, -origin.y);
  GetClientRect(window, &client);
  MapWindowPoints(window, nullptr, reinterpret_cast<POINT*>(&client), 2);
  OffsetRect(&client, -origin.x, -origin.y);
  const int saved = SaveDC(dc);
  if (!saved) return;
  ExcludeClipRect(dc, client.left, client.top, client.right, client.bottom);
  {
    // Keep the native EDIT's text/selection surface out of the frame upload.
    const PaintBuffer buffer(dc, bounds);
    DrawFieldSurface(window, buffer.dc(), bounds, background,
                     GetFocus() == window);
  }
  RestoreDC(dc, saved);
}

inline LRESULT CALLBACK FieldFrameProc(HWND window, UINT message, WPARAM wparam,
                                       LPARAM lparam, UINT_PTR subclass_id,
                                       DWORD_PTR background) {
  TrackFieldHover(window, message);
  if (message == WM_SETTEXT) SetFieldError(window, false);
  if (message == WM_NCCALCSIZE) {
    const LRESULT result = DefSubclassProc(window, message, wparam, lparam);
    RECT* client = wparam
                       ? &reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam)->rgrc[0]
                       : reinterpret_cast<RECT*>(lparam);
    // Keep the rectangular native EDIT surface beyond the rounded corners.
    // Keep the existing vertical text space and font sizing.
    const int horizontal_inset = std::max(
        0, std::min(Scale(window, kFieldFrameInset + kFieldCornerRadius),
                    static_cast<int>(client->right - client->left) / 2));
    const int vertical_inset = std::max(
        0, std::min(Scale(window, 4),
                    static_cast<int>(client->bottom - client->top) / 2));
    InflateRect(client, -horizontal_inset, -vertical_inset);
    return result;
  }
  if (message == WM_NCPAINT) {
    if (HDC dc = GetWindowDC(window)) {
      PaintFieldFrame(window, dc, static_cast<COLORREF>(background));
      ReleaseDC(window, dc);
    }
    return 0;
  }
  if (message == WM_PRINT) {
    if ((lparam & PRF_CHECKVISIBLE) && !IsWindowVisible(window)) return 0;
    const HDC dc = reinterpret_cast<HDC>(wparam);
    if (lparam & PRF_NONCLIENT)
      PaintFieldFrame(window, dc, static_cast<COLORREF>(background));
    if (lparam & PRF_CLIENT) {
      const int saved = SaveDC(dc);
      if (!saved) return 0;
      if (lparam & PRF_NONCLIENT) {
        RECT bounds{};
        POINT client{};
        GetWindowRect(window, &bounds);
        ClientToScreen(window, &client);
        OffsetViewportOrgEx(dc, client.x - bounds.left, client.y - bounds.top,
                            nullptr);
      }
      RECT client{};
      GetClientRect(window, &client);
      IntersectClipRect(dc, client.left, client.top, client.right,
                        client.bottom);
      // Native WM_PRINT would draw its rectangular nonclient frame as well.
      // Ask it for client content only, at the custom client area's origin.
      DefSubclassProc(window, WM_PRINTCLIENT, wparam, lparam & ~PRF_NONCLIENT);
      RestoreDC(dc, saved);
    }
    return 0;
  }
  if (message == WM_NCACTIVATE) {
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
    return TRUE;
  }
  if (message == WM_NCDESTROY) {
    RemoveFieldProperties(window);
    RemoveWindowSubclass(window, FieldFrameProc, subclass_id);
    return DefSubclassProc(window, message, wparam, lparam);
  }
  const LRESULT result = DefSubclassProc(window, message, wparam, lparam);
  if (message == WM_SETFONT || message == WM_DPICHANGED_AFTERPARENT) {
    SetWindowPos(window, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
  }
  if (message == WM_SETFOCUS || message == WM_KILLFOCUS ||
      message == WM_ENABLE || message == WM_UPDATEUISTATE ||
      message == WM_THEMECHANGED || message == WM_SETFONT ||
      message == WM_DPICHANGED_AFTERPARENT)
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
  return result;
}

inline void PrepareField(HWND window, COLORREF background = kPanel) {
  if (!SetWindowSubclass(window, FieldFrameProc, 4, background)) return;
  SetPropW(window, kFieldProperty, reinterpret_cast<HANDLE>(1));
  SetWindowLongPtrW(window, GWL_STYLE,
                    GetWindowLongPtrW(window, GWL_STYLE) & ~WS_BORDER);
  SetWindowLongPtrW(window, GWL_EXSTYLE,
                    GetWindowLongPtrW(window, GWL_EXSTYLE) &
                        ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
  SetWindowPos(window, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                   SWP_FRAMECHANGED);
}

inline void PaintCombo(HWND window, HDC dc, COLORREF background) {
  const int saved = SaveDC(dc);
  RECT bounds{};
  GetClientRect(window, &bounds);
  const bool enabled = IsWindowEnabled(window) != FALSE;
  const bool focused = GetFocus() == window;
  const bool dropped = SendMessageW(window, CB_GETDROPPEDSTATE, 0, 0) != 0;
  DrawFieldSurface(window, dc, bounds, background, focused || dropped);
  COMBOBOXINFO info{};
  info.cbSize = sizeof(info);
  RECT label = bounds;
  RECT arrow = bounds;
  if (GetComboBoxInfo(window, &info)) {
    label = info.rcItem;
    arrow = info.rcButton;
  } else {
    label.right -= Scale(window, 24);
    arrow.left = label.right;
  }
  label.left = std::max<LONG>(label.left, Scale(window, 8));
  label.right -= Scale(window, 4);
  std::wstring text(GetWindowTextLengthW(window) + 1, L'\0');
  text.resize(
      GetWindowTextW(window, text.data(), static_cast<int>(text.size())));
  const HFONT font =
      reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0));
  Text(dc, font, text.c_str(), label,
       enabled ? RGB(31, 41, 55) : RGB(107, 114, 128),
       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
  {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(Color(enabled ? RGB(31, 41, 55) : RGB(107, 114, 128)),
                     BorderWidth(window));
    const float x = (arrow.left + arrow.right) / 2.0f;
    const float y = (arrow.top + arrow.bottom) / 2.0f;
    const float size = 3.0f * BorderWidth(window);
    Gdiplus::PointF points[] = {{x - size, y - size / 2.0f},
                                {x, y + size / 2.0f},
                                {x + size, y - size / 2.0f}};
    graphics.DrawLines(&pen, points, 3);
  }
  RestoreDC(dc, saved);
}

inline LRESULT CALLBACK ComboFrameProc(HWND window, UINT message, WPARAM wparam,
                                       LPARAM lparam, UINT_PTR subclass_id,
                                       DWORD_PTR background) {
  TrackFieldHover(window, message);
  if (message == WM_ERASEBKGND) return 1;
  if (message == WM_PAINT) {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window, &paint);
    RECT bounds{};
    GetClientRect(window, &bounds);
    {
      const PaintBuffer buffer(dc, bounds);
      PaintCombo(window, buffer.dc(), static_cast<COLORREF>(background));
    }
    EndPaint(window, &paint);
    return 0;
  }
  if (message == WM_PRINTCLIENT) {
    PaintCombo(window, reinterpret_cast<HDC>(wparam),
               static_cast<COLORREF>(background));
    return 0;
  }
  if (message == WM_PRINT) {
    if ((lparam & PRF_CHECKVISIBLE) && !IsWindowVisible(window)) return 0;
    const LRESULT result = DefSubclassProc(
        window, message, wparam, lparam & ~(PRF_CLIENT | PRF_ERASEBKGND));
    if (lparam & PRF_CLIENT)
      PaintCombo(window, reinterpret_cast<HDC>(wparam),
                 static_cast<COLORREF>(background));
    return result;
  }
  if (message == WM_NCDESTROY) {
    RemoveFieldProperties(window);
    RemoveWindowSubclass(window, ComboFrameProc, subclass_id);
    return DefSubclassProc(window, message, wparam, lparam);
  }
  const LRESULT result = DefSubclassProc(window, message, wparam, lparam);
  // Keep native selection, keyboard navigation and the popup list behavior.
  if (message == WM_SETFOCUS || message == WM_KILLFOCUS ||
      message == WM_ENABLE || message == WM_SETTEXT || message == WM_SETFONT ||
      message == WM_UPDATEUISTATE || message == WM_THEMECHANGED ||
      message == CB_SETCURSEL || message == CB_SHOWDROPDOWN ||
      message == WM_KEYDOWN || message == WM_CHAR || message == WM_MOUSEWHEEL ||
      message == WM_LBUTTONDOWN || message == WM_LBUTTONUP)
    InvalidateRect(window, nullptr, FALSE);
  return result;
}

inline void PrepareCombo(HWND window, COLORREF background = kPanel) {
  if (SetWindowSubclass(window, ComboFrameProc, 5, background))
    SetPropW(window, kFieldProperty, reinterpret_cast<HANDLE>(1));
}

}  // namespace mouse_cursor_detector::settings_ui

#endif  // MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_CONTROL_FRAMES_H_
