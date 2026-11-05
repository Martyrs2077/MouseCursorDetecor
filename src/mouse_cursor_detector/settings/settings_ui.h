#ifndef MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_UI_H_
#define MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_UI_H_

#include <windows.h>

#include <commctrl.h>
#include <objidl.h>

#include <gdiplus.h>

#include <algorithm>
#include <string>

#include "mouse_cursor_detector/settings/settings_model.h"

namespace mouse_cursor_detector::settings_ui {
struct Palette {
  COLORREF background;
  COLORREF panel;
  COLORREF text;
  COLORREF muted;
  COLORREF border;
  COLORREF accent;
  COLORREF tint;
  COLORREF on_accent;
  int radius;
  COLORREF rail;
  COLORREF stage;
  COLORREF stage_border;
  COLORREF stage_muted;
  COLORREF panel_alt;
};

inline Palette ThemePalette(UiTheme theme) {
  switch (theme) {
    case UiTheme::kNative:
      return {RGB(242, 246, 243), RGB(255, 255, 255), RGB(22, 37, 31),
              RGB(97, 116, 106), RGB(220, 231, 223), RGB(15, 157, 110),
              RGB(225, 244, 235), RGB(255, 255, 255), 16, RGB(230, 238, 233),
              RGB(7, 27, 20), RGB(20, 53, 42), RGB(166, 184, 175),
              RGB(246, 250, 247)};
    case UiTheme::kDark:
      return {RGB(13, 17, 20), RGB(24, 31, 35), RGB(243, 240, 234),
              RGB(166, 154, 143), RGB(44, 52, 56), RGB(231, 176, 108),
              RGB(51, 40, 29), RGB(27, 18, 9), 16, RGB(18, 24, 28),
              RGB(11, 13, 14), RGB(44, 42, 39), RGB(154, 143, 132),
              RGB(19, 26, 30)};
    case UiTheme::kPaper:
      return {RGB(244, 238, 228), RGB(255, 252, 247), RGB(43, 38, 32),
              RGB(121, 111, 97), RGB(227, 216, 199), RGB(194, 90, 58),
              RGB(246, 227, 216), RGB(255, 255, 255), 16, RGB(233, 223, 208),
              RGB(33, 23, 19), RGB(74, 52, 40), RGB(191, 175, 159),
              RGB(248, 242, 232)};
    case UiTheme::kMinimal:
    default:
      return {RGB(242, 245, 249), RGB(255, 255, 255), RGB(16, 24, 39),
              RGB(100, 116, 139), RGB(225, 231, 239), RGB(47, 107, 255),
              RGB(233, 239, 255), RGB(255, 255, 255), 16, RGB(233, 237, 244),
              RGB(11, 21, 38), RGB(27, 42, 66), RGB(159, 176, 192),
              RGB(247, 249, 252)};
  }
}

// Settings and its owned dialogs run on the same UI thread and share a theme.
inline COLORREF kBackground = RGB(242, 245, 249);
inline COLORREF kPanel = RGB(255, 255, 255);
inline COLORREF kPanelAlt = RGB(247, 249, 252);
inline COLORREF kRail = RGB(233, 237, 244);
inline COLORREF kText = RGB(16, 24, 39);
inline COLORREF kMuted = RGB(100, 116, 139);
inline COLORREF kBorder = RGB(225, 231, 239);
inline COLORREF kAccent = RGB(47, 107, 255);
inline COLORREF kAccentTint = RGB(233, 239, 255);
inline COLORREF kOnAccent = RGB(255, 255, 255);
inline COLORREF kStage = RGB(11, 21, 38);
inline COLORREF kStageBorder = RGB(27, 42, 66);
inline COLORREF kStageMuted = RGB(159, 176, 192);
inline int kRadius = 16;
inline void UseTheme(UiTheme theme) {
  const auto palette = ThemePalette(theme);
  kBackground = palette.background;
  kPanel = palette.panel;
  kPanelAlt = palette.panel_alt;
  kRail = palette.rail;
  kText = palette.text;
  kMuted = palette.muted;
  kBorder = palette.border;
  kAccent = palette.accent;
  kAccentTint = palette.tint;
  kOnAccent = palette.on_accent;
  kStage = palette.stage;
  kStageBorder = palette.stage_border;
  kStageMuted = palette.stage_muted;
  kRadius = palette.radius;
}
inline constexpr UINT_PTR kAnimationTimer = 10;
inline constexpr int kMinClientWidth = 820;
inline constexpr int kMinClientHeight = 720;
inline constexpr wchar_t kButtonHotProperty[] = L"MouseCursorDetector.ButtonHot";
inline constexpr wchar_t kRedrawProperty[] = L"MouseCursorDetector.Redraw";

// Batch a visible window's layout/content changes, including nested updates.
// Initially hidden windows must stay hidden when the batch finishes.
class ScopedRedraw {
 public:
  explicit ScopedRedraw(HWND window) {
    if (!window || GetPropW(window, kRedrawProperty) ||
        !(GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE))
      return;
    if (!SetPropW(window, kRedrawProperty, reinterpret_cast<HANDLE>(1))) return;
    window_ = window;
    SendMessageW(window_, WM_SETREDRAW, FALSE, 0);
  }

  ~ScopedRedraw() {
    if (!window_ || !IsWindow(window_)) return;
    RemovePropW(window_, kRedrawProperty);
    SendMessageW(window_, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(window_, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
  }

  ScopedRedraw(const ScopedRedraw&) = delete;
  ScopedRedraw& operator=(const ScopedRedraw&) = delete;

 private:
  HWND window_ = nullptr;
};

inline LRESULT CALLBACK ButtonProc(HWND window, UINT message, WPARAM wparam,
                                   LPARAM lparam, UINT_PTR subclass_id,
                                   DWORD_PTR) {
  if (message == WM_ERASEBKGND) return 1;
  if (message == WM_MOUSEMOVE && IsWindowEnabled(window) &&
      !GetPropW(window, kButtonHotProperty)) {
    TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window, 0};
    if (TrackMouseEvent(&tracking)) {
      SetPropW(window, kButtonHotProperty, reinterpret_cast<HANDLE>(1));
      InvalidateRect(window, nullptr, FALSE);
    }
  }
  if (message == WM_MOUSELEAVE || message == WM_ENABLE) {
    RemovePropW(window, kButtonHotProperty);
    InvalidateRect(window, nullptr, FALSE);
  }
  if (message == WM_GETDLGCODE) {
    return DLGC_BUTTON |
           (GetDlgCtrlID(window) == IDOK ? DLGC_DEFPUSHBUTTON
                                         : DLGC_UNDEFPUSHBUTTON) |
           (wparam == VK_RETURN ? DLGC_WANTMESSAGE : 0);
  }
  if (message == BM_SETSTYLE) {
    // The dialog manager must not turn an owner-drawn button into a native one.
    if (lparam) InvalidateRect(window, nullptr, FALSE);
    return 0;
  }
  if (message == WM_KEYDOWN && wparam == VK_RETURN) {
    SendMessageW(window, BM_CLICK, 0, 0);
    return 0;
  }
  if (message == WM_CHAR && wparam == VK_RETURN) return 0;
  if (message == WM_NCDESTROY) {
    RemovePropW(window, kButtonHotProperty);
    RemoveWindowSubclass(window, ButtonProc, subclass_id);
  }
  return DefSubclassProc(window, message, wparam, lparam);
}

inline void PrepareButton(HWND window) {
  SetWindowSubclass(window, ButtonProc, 2, 0);
}

inline int Scale(HWND window, int value) {
  return MulDiv(value, GetDpiForWindow(window), 96);
}

inline float BorderWidth(HWND window) {
  return GetDpiForWindow(window) / 96.0f;
}

inline void Fill(HDC dc, const RECT& rect, COLORREF color) {
  const COLORREF previous = SetDCBrushColor(dc, color);
  FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
  SetDCBrushColor(dc, previous);
}

// Present a complete surface through GDI so the destination DC's update/child
// clipping also applies to GDI+ drawing. Fall back to direct paint on failure.
class PaintBuffer {
 public:
  PaintBuffer(HDC target, const RECT& bounds)
      : target_(target), bounds_(bounds) {
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) return;
    memory_ = CreateCompatibleDC(target);
    if (!memory_) return;
    bitmap_ = CreateCompatibleBitmap(target, width, height);
    if (bitmap_) previous_ = SelectObject(memory_, bitmap_);
    if (!previous_ || previous_ == HGDI_ERROR) {
      if (bitmap_) DeleteObject(bitmap_);
      DeleteDC(memory_);
      bitmap_ = nullptr;
      memory_ = nullptr;
      return;
    }
    SetWindowOrgEx(memory_, bounds.left, bounds.top, nullptr);
  }

  ~PaintBuffer() {
    if (!memory_) return;
    BitBlt(target_, bounds_.left, bounds_.top, bounds_.right - bounds_.left,
           bounds_.bottom - bounds_.top, memory_, bounds_.left, bounds_.top,
           SRCCOPY);
    SelectObject(memory_, previous_);
    DeleteObject(bitmap_);
    DeleteDC(memory_);
  }

  HDC dc() const { return memory_ ? memory_ : target_; }
  PaintBuffer(const PaintBuffer&) = delete;
  PaintBuffer& operator=(const PaintBuffer&) = delete;

 private:
  HDC target_;
  RECT bounds_;
  HDC memory_ = nullptr;
  HBITMAP bitmap_ = nullptr;
  HGDIOBJ previous_ = nullptr;
};

inline void ShowControl(HWND control, bool visible) {
  if (!control) return;
  const bool shown = (GetWindowLongPtrW(control, GWL_STYLE) & WS_VISIBLE) != 0;
  if (shown != visible) ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
}

inline void SetControlText(HWND window, int id, const wchar_t* text) {
  const HWND control = GetDlgItem(window, id);
  std::wstring previous(GetWindowTextLengthW(control) + 1, L'\0');
  previous.resize(GetWindowTextW(control, previous.data(),
                                 static_cast<int>(previous.size())));
  if (previous != text) SetWindowTextW(control, text);
}

inline RECT ClientDip(HWND window) {
  RECT rect{};
  GetClientRect(window, &rect);
  const UINT dpi = GetDpiForWindow(window);
  rect.right = MulDiv(rect.right, 96, dpi);
  rect.bottom = MulDiv(rect.bottom, 96, dpi);
  return rect;
}

inline LRESULT CALLBACK TextTipProc(HWND window, UINT message, WPARAM wparam,
                                    LPARAM lparam, UINT_PTR subclass_id,
                                    DWORD_PTR reference) {
  if (message == WM_NOTIFY &&
      reinterpret_cast<NMHDR*>(lparam)->code == TTN_GETDISPINFOW) {
    // Keep the complete text alive while the tooltip measures and paints it.
    static thread_local std::wstring text;
    text.resize(GetWindowTextLengthW(window) + 1);
    const int length =
        GetWindowTextW(window, text.data(), static_cast<int>(text.size()));
    text.resize(length);
    reinterpret_cast<NMTTDISPINFOW*>(lparam)->lpszText = text.data();
    return 0;
  }
  if (message == WM_NCDESTROY) {
    DestroyWindow(reinterpret_cast<HWND>(reference));
    RemoveWindowSubclass(window, TextTipProc, subclass_id);
  }
  return DefSubclassProc(window, message, wparam, lparam);
}

inline void PrepareTextTip(HWND control) {
  HWND tooltip =
      CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                      WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT,
                      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, control,
                      nullptr, GetModuleHandleW(nullptr), nullptr);
  if (!tooltip) return;
  if (!SetWindowSubclass(control, TextTipProc, 3,
                         reinterpret_cast<DWORD_PTR>(tooltip))) {
    DestroyWindow(tooltip);
    return;
  }
  TOOLINFOW tool{};
  tool.cbSize = sizeof(tool);
  tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
  tool.hwnd = control;
  tool.uId = reinterpret_cast<UINT_PTR>(control);
  tool.lpszText = LPSTR_TEXTCALLBACKW;
  SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
  SendMessageW(tooltip, TTM_SETMAXTIPWIDTH, 0, Scale(control, 480));
  SendMessageW(tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 15000);
}

inline void PlaceWindow(HWND window, HWND control, int x, int y, int width,
                        int height) {
  RECT previous{};
  GetWindowRect(control, &previous);
  MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&previous), 2);
  const int left = Scale(window, x);
  const int top = Scale(window, y);
  const int width_px = Scale(window, std::max(1, width));
  const int height_px = Scale(window, std::max(1, height));
  if (previous.left == left && previous.top == top &&
      previous.right - previous.left == width_px &&
      previous.bottom - previous.top == height_px)
    return;
  SetWindowPos(control, nullptr, left, top, width_px, height_px,
               SWP_NOZORDER | SWP_NOACTIVATE);
}

inline void Place(HWND window, int id, int x, int y, int width, int height) {
  PlaceWindow(window, GetDlgItem(window, id), x, y, width, height);
}

inline Gdiplus::Color Color(COLORREF color) {
  return Gdiplus::Color(255, GetRValue(color), GetGValue(color),
                        GetBValue(color));
}

inline void RoundedBox(HWND window, HDC dc, const RECT& rect, COLORREF fill,
                       COLORREF border, int radius, bool fill_area = true,
                       COLORREF bottom_fill = CLR_INVALID) {
  if (rect.right <= rect.left || rect.bottom <= rect.top) return;
  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  const float stroke = BorderWidth(window);
  const float x = rect.left + stroke / 2.0f;
  const float y = rect.top + stroke / 2.0f;
  const float width = static_cast<float>(rect.right - rect.left) - stroke;
  const float height = static_cast<float>(rect.bottom - rect.top) - stroke;
  if (width <= 0 || height <= 0) return;
  const float diameter =
      std::min(static_cast<float>(radius * 2), std::min(width, height));
  Gdiplus::GraphicsPath path;
  path.AddArc(x, y, diameter, diameter, 180.0f, 90.0f);
  path.AddArc(x + width - diameter, y, diameter, diameter, 270.0f, 90.0f);
  path.AddArc(x + width - diameter, y + height - diameter, diameter, diameter,
              0.0f, 90.0f);
  path.AddArc(x, y + height - diameter, diameter, diameter, 90.0f, 90.0f);
  path.CloseFigure();
  Gdiplus::SolidBrush brush(Color(fill));
  Gdiplus::Pen pen(Color(border), stroke);
  if (fill_area) {
    if (bottom_fill != CLR_INVALID) {
      Gdiplus::LinearGradientBrush gradient(
          Gdiplus::PointF(x, y), Gdiplus::PointF(x, y + height), Color(fill),
          Color(bottom_fill));
      graphics.FillPath(&gradient, &path);
    } else {
      graphics.FillPath(&brush, &path);
    }
  }
  graphics.DrawPath(&pen, &path);
}

inline void Panel(HWND window, HDC dc, int x, int y, int width, int height) {
  RECT rect{Scale(window, x), Scale(window, y), Scale(window, x + width),
            Scale(window, y + height)};
  RoundedBox(window, dc, rect, kPanel, kBorder, Scale(window, kRadius));
}

inline void HorizontalRule(HWND window, HDC dc, int left, int right, int y) {
  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  const float stroke = BorderWidth(window);
  Gdiplus::Pen pen(Color(kBorder), stroke);
  graphics.DrawLine(&pen, static_cast<float>(left), y + stroke / 2.0f,
                    static_cast<float>(right), y + stroke / 2.0f);
}

inline HBRUSH ControlBrush(HDC dc, COLORREF background = kPanel,
                           COLORREF foreground = kText) {
  SetBkColor(dc, background);
  SetTextColor(dc, foreground);
  SetDCBrushColor(dc, background);
  return static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
}

inline void Text(HDC dc, HFONT font, const wchar_t* text, RECT rect,
                 COLORREF color = kText,
                 UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
  const int old_mode = SetBkMode(dc, TRANSPARENT);
  const auto old_color = SetTextColor(dc, color);
  const auto old_font = font ? SelectObject(dc, font) : nullptr;
  DrawTextW(dc, text, -1, &rect, flags);
  if (old_font) SelectObject(dc, old_font);
  SetTextColor(dc, old_color);
  SetBkMode(dc, old_mode);
}

inline void Button(const DRAWITEMSTRUCT& item, bool primary = false,
                   COLORREF background = kPanel, bool compact = false) {
  const bool disabled = (item.itemState & ODS_DISABLED) != 0;
  const bool pressed = (item.itemState & ODS_SELECTED) != 0;
  const bool hot = GetPropW(item.hwndItem, kButtonHotProperty) != nullptr;
  const bool focused =
      (item.itemState & ODS_FOCUS) && !(item.itemState & ODS_NOFOCUSRECT);
  const COLORREF fill =
      disabled ? kPanelAlt
      : primary ? (pressed ? kText : kAccent)
               : (pressed || hot ? kAccentTint : kPanel);
  const COLORREF border =
      disabled ? kBorder : primary ? kAccent : (pressed || hot ? kAccent : kBorder);
  const COLORREF foreground =
      disabled ? kMuted
      : primary ? kOnAccent
                : (pressed || hot ? kAccent : kText);
  Fill(item.hDC, item.rcItem, background);
  RECT face = item.rcItem;
  InflateRect(&face, -Scale(item.hwndItem, 1), -Scale(item.hwndItem, 1));
  const int radius = compact ? Scale(item.hwndItem, 6) : Scale(item.hwndItem, 9);
  RoundedBox(item.hwndItem, item.hDC, face, fill, border, radius);
  wchar_t label[256]{};
  GetWindowTextW(item.hwndItem, label, 256);
  const auto font =
      reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0));
  Text(item.hDC, font, label, face, foreground,
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  if (focused) {
    RECT focus = face;
    InflateRect(&focus, -Scale(item.hwndItem, 4), -Scale(item.hwndItem, 4));
    const COLORREF old_text = SetTextColor(item.hDC, foreground);
    const COLORREF old_background = SetBkColor(item.hDC, fill);
    DrawFocusRect(item.hDC, &focus);
    SetTextColor(item.hDC, old_text);
    SetBkColor(item.hDC, old_background);
  }
}
}  // namespace mouse_cursor_detector::settings_ui

#endif  // MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_UI_H_
