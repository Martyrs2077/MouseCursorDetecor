#include "mouse_cursor_detector/overlay/native_cursor_window.h"

#include <objidl.h>

#include <gdiplus.h>

#include <cmath>

namespace mouse_cursor_detector {
namespace {
constexpr wchar_t kOverlayClass[] = L"MouseCursorDetector.Overlay.v1";
}  // namespace

bool NativeCursorWindow::Create() {
  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.hInstance = GetModuleHandleW(nullptr);
  window_class.lpfnWndProc = DefWindowProcW;
  window_class.lpszClassName = kOverlayClass;
  if (!RegisterClassExW(&window_class) &&
      GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return false;
  }
  window_ =
      CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
                          WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                      kOverlayClass, L"", WS_POPUP, 0, 0, 1, 1, nullptr,
                      nullptr, window_class.hInstance, nullptr);
  return window_ != nullptr;
}

bool NativeCursorWindow::ApplyStyle(const Settings& settings, UINT dpi,
                                    size_t frame_index) {
  const auto& style = settings.active_style();
  const float scale = dpi / 96.0f;
  const float radius = style.radius_dip * scale;
  const float thickness = style.thickness_dip * scale;
  const int size = static_cast<int>(
      std::ceil(2.0f * (radius + thickness / 2.0f + 3.0f * scale)));
  int width = size;
  int height = size;
  if (settings.is_custom()) {
    if (!settings.image_asset || settings.image_asset->frames.empty())
      return false;
    const auto& asset = *settings.image_asset;
    const float ratio = 2.0f * radius / std::max(asset.width, asset.height);
    width = std::max(1, static_cast<int>(std::ceil(asset.width * ratio)));
    height = std::max(1, static_cast<int>(std::ceil(asset.height * ratio)));
  }
  void* pixels = nullptr;
  if (!bitmap_ || width_px_ != width || height_px_ != height) {
    ReleaseSurface();
    width_px_ = width;
    height_px_ = height;
    memory_dc_ = CreateCompatibleDC(nullptr);
    if (!memory_dc_) return false;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap_ = CreateDIBSection(memory_dc_, &info, DIB_RGB_COLORS, &pixels,
                               nullptr, 0);
    if (!bitmap_ || !pixels) return false;
    original_bitmap_ = SelectObject(memory_dc_, bitmap_);
    if (!original_bitmap_ || original_bitmap_ == HGDI_ERROR) return false;
  } else {
    DIBSECTION section{};
    if (!GetObjectW(bitmap_, sizeof(section), &section)) return false;
    pixels = section.dsBm.bmBits;
  }
  // Explicit PARGB pixels: GDI+ performs premultiplication, never do it twice.
  Gdiplus::Bitmap surface(width, height, width * 4, PixelFormat32bppPARGB,
                          static_cast<BYTE*>(pixels));
  Gdiplus::Graphics graphics(&surface);
  if (graphics.GetLastStatus() != Gdiplus::Ok) return false;
  graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  const Gdiplus::Color color(
      static_cast<BYTE>(style.opacity_percent * 255 / 100),
      GetRValue(style.color), GetGValue(style.color), GetBValue(style.color));
  const float center = size / 2.0f;
  const Gdiplus::RectF bounds(center - radius, center - radius, radius * 2,
                              radius * 2);
  Gdiplus::Status result;
  if (settings.is_custom()) {
    const auto& asset = *settings.image_asset;
    const auto& frame = asset.frames[frame_index % asset.frames.size()];
    Gdiplus::Bitmap image(asset.width, asset.height, asset.width * 4,
                          PixelFormat32bppPARGB,
                          const_cast<BYTE*>(frame.pixels.data()));
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    result = graphics.DrawImage(&image, 0, 0, width, height);
  } else if (settings.mode == IndicatorMode::kHighlight) {
    Gdiplus::SolidBrush brush(color);
    result = graphics.FillEllipse(&brush, bounds);
  } else {
    Gdiplus::Pen pen(color, thickness);
    result = graphics.DrawEllipse(&pen, bounds);
  }
  if (result != Gdiplus::Ok) return false;
  graphics.Flush(Gdiplus::FlushIntentionSync);
  SIZE dimensions{width, height};
  POINT source{0, 0};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  if (settings.is_custom()) {
    blend.SourceConstantAlpha =
        static_cast<BYTE>(style.opacity_percent * 255 / 100);
  }
  return UpdateLayeredWindow(window_, nullptr, nullptr, &dimensions, memory_dc_,
                             &source, 0, &blend, ULW_ALPHA) != 0;
}

void NativeCursorWindow::FollowCursor(POINT point) {
  SetWindowPos(window_, nullptr, point.x - width_px_ / 2,
               point.y - height_px_ / 2, 0, 0,
               SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
}

void NativeCursorWindow::SetVisible(bool visible) {
  ShowWindow(window_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
  if (visible) ReassertTopmost();
}

void NativeCursorWindow::ReassertTopmost() {
  SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void NativeCursorWindow::ReleaseSurface() {
  if (memory_dc_ && original_bitmap_ && original_bitmap_ != HGDI_ERROR) {
    SelectObject(memory_dc_, original_bitmap_);
  }
  original_bitmap_ = nullptr;
  if (bitmap_) DeleteObject(bitmap_);
  if (memory_dc_) DeleteDC(memory_dc_);
  bitmap_ = nullptr;
  memory_dc_ = nullptr;
}

NativeCursorWindow::~NativeCursorWindow() {
  if (window_) DestroyWindow(window_);
  ReleaseSurface();
}
}  // namespace mouse_cursor_detector
