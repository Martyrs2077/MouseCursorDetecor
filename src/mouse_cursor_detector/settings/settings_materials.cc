#include "mouse_cursor_detector/settings/settings_window.h"

#include <commctrl.h>
#include <objidl.h>
#include <shellscalingapi.h>
#include <windowsx.h>

#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <exception>
#include <string>

#include "mouse_cursor_detector/resources/resource.h"
#include "mouse_cursor_detector/settings/settings_control_frames.h"
#include "mouse_cursor_detector/settings/settings_control_ids.h"
#include "mouse_cursor_detector/settings/settings_ui.h"

namespace mouse_cursor_detector {
namespace {
constexpr UINT_PTR kPreviewTimer = settings_ui::kAnimationTimer;
}  // namespace

SettingsWindow::PreviewGeometry SettingsWindow::GetPreviewGeometry(
    HWND control) const {
  RECT bounds{};
  GetClientRect(control, &bounds);
  const auto px = [control](int dip) {
    return settings_ui::Scale(control, dip);
  };
  const bool material = GetDlgCtrlID(control) == IDC_MATERIAL_PREVIEW;
  PreviewGeometry geometry;
  geometry.area =
      material
          ? RECT{px(16), px(16), bounds.right - px(16), bounds.bottom - px(16)}
          : RECT{px(17), px(45), bounds.right - px(17), bounds.bottom - px(46)};
  geometry.center_x = (geometry.area.left + geometry.area.right) / 2.0f;
  geometry.center_y = (geometry.area.top + geometry.area.bottom) / 2.0f;
  const float available_radius =
      std::max(1.0f, std::min(geometry.area.right - geometry.area.left,
                              geometry.area.bottom - geometry.area.top) /
                         2.0f);
  geometry.scale =
      available_radius / (material || draft_.is_custom() ? 256.0f : 110.0f);
  return geometry;
}

void SettingsWindow::PreparePreview(HWND control) {
  SetWindowLongPtrW(control, GWL_STYLE,
                    GetWindowLongPtrW(control, GWL_STYLE) | SS_NOTIFY);
  if (!SetWindowSubclass(control, PreviewProc, 1,
                         reinterpret_cast<DWORD_PTR>(this)))
    MessageBoxW(window_, Tr(L"无法初始化预览拖动，请关闭设置后重试。"),
                Tr(L"鼠标提示"), MB_OK | MB_ICONERROR);
}

bool SettingsWindow::RefreshPreviewCursor() {
  CURSORINFO info{sizeof(info)};
  const bool available = GetCursorInfo(&info) && info.hCursor;
  const HCURSOR cursor =
      available ? info.hCursor : LoadCursorW(nullptr, IDC_ARROW);
  UINT dpi_x = 96;
  UINT dpi_y = 96;
  if (available &&
      FAILED(GetDpiForMonitor(
          MonitorFromPoint(info.ptScreenPos, MONITOR_DEFAULTTONEAREST),
          MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y)))
    dpi_x = 96;
  const bool changed =
      cursor != preview_cursor_ || dpi_x != preview_cursor_dpi_;
  preview_cursor_ = cursor;
  preview_cursor_dpi_ = dpi_x;
  return changed;
}

void SettingsWindow::DrawPreviewCursor(HDC dc, HWND control) {
  RefreshPreviewCursor();
  ICONINFO info{};
  if (!preview_cursor_ || !GetIconInfo(preview_cursor_, &info)) return;
  BITMAP bitmap{};
  const bool color = info.hbmColor != nullptr;
  const bool valid = GetObjectW(color ? info.hbmColor : info.hbmMask,
                                sizeof(bitmap), &bitmap) != 0;
  if (info.hbmColor) DeleteObject(info.hbmColor);
  if (info.hbmMask) DeleteObject(info.hbmMask);
  if (!valid || bitmap.bmWidth <= 0 || bitmap.bmHeight <= 0) return;
  const auto geometry = GetPreviewGeometry(control);
  // The cursor bitmap is already in screen pixels. Scale it by the same
  // preview-to-screen ratio as the image, and anchor its actual hotspot.
  const float ratio = geometry.scale * 96.0f / preview_cursor_dpi_;
  const int width =
      std::max(1, static_cast<int>(std::lround(bitmap.bmWidth * ratio)));
  const int height = std::max(
      1, static_cast<int>(std::lround(
             (color ? bitmap.bmHeight : bitmap.bmHeight / 2) * ratio)));
  const int saved = SaveDC(dc);
  if (!saved) return;
  IntersectClipRect(dc, geometry.area.left, geometry.area.top,
                    geometry.area.right, geometry.area.bottom);
  DrawIconEx(
      dc,
      static_cast<int>(std::lround(geometry.center_x - info.xHotspot * ratio)),
      static_cast<int>(std::lround(geometry.center_y - info.yHotspot * ratio)),
      preview_cursor_, width, height, 0, nullptr, DI_NORMAL);
  RestoreDC(dc, saved);
}

void SettingsWindow::DragPreview(HWND control, POINT point) {
  const auto geometry = GetPreviewGeometry(control);
  const auto coordinate = [](float value, float minimum, float maximum) {
    return static_cast<int>(std::lround(std::clamp(value, minimum, maximum)));
  };
  // Keep the image center in the stage so some of it remains draggable.
  const float x =
      drag_offset_.x_dip + (point.x - drag_origin_.x) / geometry.scale;
  const float y =
      drag_offset_.y_dip + (point.y - drag_origin_.y) / geometry.scale;
  const ImageOffset offset{
      std::clamp(
          coordinate(
              x, (geometry.area.left - geometry.center_x) / geometry.scale,
              (geometry.area.right - geometry.center_x) / geometry.scale),
          -kMaxImageOffsetDip, kMaxImageOffsetDip),
      std::clamp(
          coordinate(
              y, (geometry.area.top - geometry.center_y) / geometry.scale,
              (geometry.area.bottom - geometry.center_y) / geometry.scale),
          -kMaxImageOffsetDip, kMaxImageOffsetDip)};
  const auto found = draft_.image_offsets.find(draft_.style_name);
  if (found == draft_.image_offsets.end()) return;
  found->second = offset;
  PreviewDraft();
  InvalidateRect(control, nullptr, FALSE);
  UpdateWindow(control);
}

LRESULT CALLBACK SettingsWindow::PreviewProc(HWND window, UINT message,
                                             WPARAM wparam, LPARAM lparam,
                                             UINT_PTR subclass_id,
                                             DWORD_PTR reference) {
  auto* self = reinterpret_cast<SettingsWindow*>(reference);
  try {
    if (message == WM_LBUTTONDOWN && !self->MaterialBusy() &&
        !self->naming_style_ && self->draft_.is_custom() &&
        !self->draft_.style_name.empty() && self->draft_.image_asset) {
      const auto geometry = self->GetPreviewGeometry(window);
      const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      if (PtInRect(&geometry.area, point)) {
        // Permit grabbing the stage as well as transparent image pixels.
        // This also recovers a saved position after the viewport is resized.
        const auto inserted = self->draft_.image_offsets.try_emplace(
            self->draft_.style_name, ImageOffset{});
        self->drag_offset_ = inserted.first->second;
        self->drag_origin_ = point;
        self->dragged_preview_ = window;
        SetCapture(window);
        if (GetCapture() != window) self->dragged_preview_ = nullptr;
        return 0;
      }
    }
    if (self->dragged_preview_ == window) {
      if (message == WM_MOUSEMOVE || message == WM_LBUTTONUP) {
        self->DragPreview(window,
                          POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        if (message == WM_LBUTTONUP) ReleaseCapture();
        return 0;
      }
      if (message == WM_CAPTURECHANGED) self->dragged_preview_ = nullptr;
      if (message == WM_CANCELMODE || message == WM_SIZE ||
          (message == WM_SHOWWINDOW && !wparam) || message == WM_NCDESTROY) {
        self->dragged_preview_ = nullptr;
        if (GetCapture() == window) ReleaseCapture();
      }
    }
  } catch (const std::exception&) {
    self->dragged_preview_ = nullptr;
    if (GetCapture() == window) ReleaseCapture();
    MessageBoxW(self->window_, Tr(L"无法调整预览位置，请重试。"),
                Tr(L"鼠标提示"), MB_OK | MB_ICONERROR);
    return 0;
  }
  if (message == WM_NCDESTROY)
    RemoveWindowSubclass(window, PreviewProc, subclass_id);
  return DefSubclassProc(window, message, wparam, lparam);
}

void SettingsWindow::ShowMaterials() {
  if (MaterialBusy() || !ReadControls(true)) return;
  if (materials_window_) {
    BeginLibraryRefresh();
    return;
  }
  materials_before_ = draft_;
  materials_previous_preview_ = preview_settings_;
  HWND result = nullptr;
  {
    const settings_ui::ScopedRedraw redraw(window_);
    result = CreateDialogParamW(GetModuleHandleW(nullptr),
                                MAKEINTRESOURCEW(IDD_MATERIALS), window_,
                                MaterialsProc, reinterpret_cast<LPARAM>(this));
    if (result) {
      page_ = Page::kMaterials;
      scroll_dip_ = 0;
      horizontal_scroll_dip_ = 0;
      LayoutControls();
    }
  }
  if (!result) {
    MessageBoxW(window_, Tr(L"无法创建导入光圈样式窗口。"), Tr(L"鼠标提示"),
                MB_OK | MB_ICONERROR);
    return;
  }
  SetFocus(GetDlgItem(materials_window_, IDC_MATERIAL_PATH));
  BeginLibraryRefresh();
}

void SettingsWindow::CloseMaterials(bool save, Page next_page) {
  if (!materials_window_ || MaterialBusy() || naming_style_) return;
  if (save) {
    if (!ReadControls(true) || !commit_(draft_)) return;
    original_ = draft_;
    preview_settings_ = draft_;
  }
  {
    const settings_ui::ScopedRedraw redraw(window_);
    if (!save) {
      draft_ = materials_before_;
      preview_settings_ = materials_previous_preview_;
      preview_(preview_settings_);
    }
    DestroyWindow(materials_window_);
    // Navigate straight to the requested page without painting main in between.
    page_ = next_page;
    scroll_dip_ = 0;
    horizontal_scroll_dip_ = 0;
    if (save) PreviewDraft();
    LoadControls();
  }
  SetFocus(GetDlgItem(window_, next_page == Page::kThemes
                                   ? settings_controls::kThemesTab
                                   : settings_controls::kMainTab));
}

INT_PTR CALLBACK SettingsWindow::MaterialsProc(HWND window, UINT message,
                                               WPARAM wparam, LPARAM lparam) {
  auto* self =
      reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(window, DWLP_USER));
  if (message == WM_INITDIALOG) {
    self = reinterpret_cast<SettingsWindow*>(lparam);
    SetWindowLongPtrW(window, DWLP_USER, lparam);
    self->materials_window_ = window;
    // Ant Design primary Button adaptation: use the same renderer as Save.
    const HWND language = CreateWindowExW(
        0, L"BUTTON", L"Language/语言",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 1, 1, window,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(settings_controls::kLanguage)),
        GetModuleHandleW(nullptr), nullptr);
    settings_ui::PrepareButton(language);
    settings_ui::PrepareField(GetDlgItem(window, IDC_MATERIAL_PATH),
                              settings_ui::kPanel);
    SetDialogDpiChangeBehavior(window, DDC_DISABLE_ALL, DDC_DISABLE_ALL);
    self->preview_started_ = GetTickCount64();
    self->PreparePreview(GetDlgItem(window, IDC_MATERIAL_PREVIEW));
    SetWindowTextW(window, Tr(L"素材库"));
    SetDlgItemTextW(window, IDC_MATERIAL_TITLE, Tr(L"素材库"));
    SetDlgItemTextW(window, IDC_MATERIAL_PATH_LABEL, Tr(L"导入新素材"));
    SetDlgItemTextW(window, IDC_MATERIAL_BROWSE, Tr(L"浏览"));
    SetDlgItemTextW(window, IDC_MATERIAL_IMPORT, Tr(L"导入素材"));
    SetDlgItemTextW(window, IDC_MATERIAL_PREVIEW_LABEL, Tr(L"素材预览"));
    SetDlgItemTextW(window, IDC_MATERIAL_IMPORT_HINT, Tr(L"等待导入"));
    SetDlgItemTextW(window, IDC_MATERIAL_FORMATS, Tr(L"最大 20 MiB"));
    SetDlgItemTextW(window, IDC_MATERIAL_PREVIEW_HINT, Tr(L"等待导入"));
    SetDlgItemTextW(window, IDC_MATERIAL_HINT,
                    Tr(L"素材会复制到 styles/，原文件可删除"));
    SetDlgItemTextW(window, IDOK, Tr(L"保存配置"));
    SetDlgItemTextW(window, IDC_MATERIAL_REFRESH, Tr(L"刷新"));
    ShowWindow(GetDlgItem(window, IDC_MATERIAL_STATUS), SW_HIDE);
    ShowWindow(GetDlgItem(window, IDC_MATERIAL_DETAILS), SW_HIDE);
    self->material_list_entries_.clear();
    SetDlgItemTextW(window, IDCANCEL, Tr(L"取消"));
    SendDlgItemMessageW(window, IDC_MATERIAL_PATH, EM_SETLIMITTEXT, 32767, 0);
    SendDlgItemMessageW(
        window, IDC_MATERIAL_PATH, EM_SETCUEBANNER, TRUE,
        reinterpret_cast<LPARAM>(Tr(L"选择文件，或粘贴完整路径")));
    self->CreateMaterialFonts();
    for (const int id : {IDC_MATERIAL_BROWSE, IDC_MATERIAL_IMPORT,
                         IDC_MATERIAL_REFRESH, IDOK, IDCANCEL})
      settings_ui::PrepareButton(GetDlgItem(window, id));
    for (const int id :
         {IDC_MATERIAL_PATH, IDC_MATERIAL_STATUS, IDC_MATERIAL_DETAILS})
      settings_ui::PrepareTextTip(GetDlgItem(window, id));
    SendMessageW(window, DM_SETDEFID, IDOK, 0);
    self->LayoutMaterials();
    self->LoadMaterialControls();
    // If a timer cannot be created, the preview still displays its first frame.
    SetTimer(window, kPreviewTimer, 33, nullptr);
    return TRUE;
  }
  if (!self) return FALSE;
  switch (message) {
    case WM_ERASEBKGND:
      return TRUE;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(window, &paint);
      RECT bounds{};
      GetClientRect(window, &bounds);
      {
        const settings_ui::PaintBuffer buffer(dc, bounds);
        self->PaintMaterials(buffer.dc());
      }
      EndPaint(window, &paint);
      return TRUE;
    }
    case WM_SIZE:
      self->LayoutMaterials();
      return TRUE;
    case WM_DPICHANGED: {
      self->CreateMaterialFonts();
      self->LayoutMaterials();
      return TRUE;
    }
    case WM_CTLCOLORLISTBOX:
      return reinterpret_cast<INT_PTR>(
          settings_ui::ControlBrush(reinterpret_cast<HDC>(wparam),
                                    settings_ui::kPanel, settings_ui::kText));
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
      const HWND control = reinterpret_cast<HWND>(lparam);
      if (GetPropW(control, settings_ui::kFieldProperty))
        return reinterpret_cast<INT_PTR>(settings_ui::FieldBrush(
            control, reinterpret_cast<HDC>(wparam)));
      const int id = GetDlgCtrlID(control);
      const bool muted =
          id == IDC_MATERIAL_FORMATS || id == IDC_MATERIAL_DETAILS ||
          id == IDC_MATERIAL_HINT || id == IDC_MATERIAL_PREVIEW_HINT ||
          id == IDC_MATERIAL_IMPORT_HINT;
      const bool on_background = id == IDC_MATERIAL_TITLE ||
                                 id == IDC_MATERIAL_HINT;
      const auto background = on_background ? settings_ui::kBackground
                                            : settings_ui::kPanel;
      return reinterpret_cast<INT_PTR>(settings_ui::ControlBrush(
          reinterpret_cast<HDC>(wparam), background,
          muted ? settings_ui::kMuted : settings_ui::kText));
    }
    case WM_COMMAND: {
      const int id = LOWORD(wparam);
      if (id == settings_controls::kLanguage) {
        self->ToggleLanguage();
        return TRUE;
      }
      if (id == IDC_MATERIAL_PATH && HIWORD(wparam) == EN_CHANGE) {
        settings_ui::SetFieldError(GetDlgItem(window, id), false);
        return TRUE;
      }
      if (self->naming_style_) return TRUE;
      if (id == IDC_MATERIAL_LIST && HIWORD(wparam) == LBN_SELCHANGE) {
        if (self->MaterialBusy() || self->loading_) return TRUE;
        const auto index = SendDlgItemMessageW(window, id, LB_GETCURSEL, 0, 0);
        if (index >= 0 && self->ReadControls(true))
          self->BeginSelectStyle(static_cast<size_t>(index));
        return TRUE;
      }
      if (id == IDC_MATERIAL_REFRESH) {
        self->BeginLibraryRefresh();
        return TRUE;
      }
      if (id == IDC_MATERIAL_BROWSE) {
        self->BrowseStyle();
        return TRUE;
      }
      if (id == IDC_MATERIAL_IMPORT) {
        self->BeginImport();
        return TRUE;
      }
      if (id != IDOK && id != IDCANCEL) return FALSE;
      if (self->MaterialBusy()) return TRUE;
      self->CloseMaterials(id == IDOK);
      return TRUE;
    }
    case WM_CLOSE:
      if (self->naming_style_) return TRUE;
      if (self->MaterialBusy()) {
        MessageBoxW(window, Tr(L"正在导入素材，请稍候再关闭。"),
                    Tr(L"导入光圈样式"), MB_OK);
      } else {
        self->CloseMaterials(false);
      }
      return TRUE;
    case WM_TIMER:
      if (wparam == kPreviewTimer && IsWindowVisible(self->window_) &&
          !IsIconic(self->window_)) {
        const bool changed = self->RefreshPreviewCursor();
        if (changed || (self->draft_.image_asset &&
                        self->draft_.image_asset->is_gif))
          InvalidateRect(GetDlgItem(window, IDC_MATERIAL_PREVIEW), nullptr,
                         FALSE);
        return TRUE;
      }
      break;
    case WM_DRAWITEM: {
      const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
      if (!item) return FALSE;
      if (item->CtlID == IDC_MATERIAL_LIST) {
        self->DrawMaterialListItem(*item);
        return TRUE;
      }
      if (wparam == IDC_MATERIAL_PREVIEW) {
        self->DrawMaterialPreview(*item);
        return TRUE;
      }
      if (item->CtlType == ODT_BUTTON) {
        const settings_ui::PaintBuffer buffer(item->hDC, item->rcItem);
        DRAWITEMSTRUCT buffered = *item;
        buffered.hDC = buffer.dc();
        const bool in_panel = item->CtlID == IDC_MATERIAL_IMPORT ||
                              item->CtlID == IDC_MATERIAL_BROWSE ||
                              item->CtlID == IDC_MATERIAL_REFRESH;
        settings_ui::Button(
            buffered,
            item->CtlID == IDOK || item->CtlID == IDC_MATERIAL_IMPORT ||
                item->CtlID == settings_controls::kLanguage,
            in_panel ? settings_ui::kPanel : settings_ui::kBackground,
            item->CtlID == IDC_MATERIAL_BROWSE);
        return TRUE;
      }
      break;
    }
    case WM_DESTROY:
      self->material_list_entries_.clear();
      self->material_thumbnails_.clear();
      KillTimer(window, kPreviewTimer);
      for (HFONT& font : self->material_fonts_) {
        if (font) DeleteObject(font);
        font = nullptr;
      }
      self->materials_window_ = nullptr;
      return TRUE;
  }
  return FALSE;
}

void SettingsWindow::CreateMaterialFonts() {
  const UINT dpi = GetDpiForWindow(materials_window_);
  std::array<HFONT, 4> fonts{};
  const int sizes[] = {10, 11, 18, 9};
  for (size_t i = 0; i < fonts.size(); ++i) {
    fonts[i] = CreateFontW(-MulDiv(sizes[i], dpi, 72), 0, 0, 0,
                           i == 1 || i == 2 ? FW_MEDIUM : FW_NORMAL, FALSE,
                           FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH, L"Microsoft YaHei UI");
  }
  for (HWND child = GetWindow(materials_window_, GW_CHILD); child;
       child = GetWindow(child, GW_HWNDNEXT)) {
    const int id = GetDlgCtrlID(child);
    const bool small = id == IDC_MATERIAL_FORMATS ||
                       id == IDC_MATERIAL_DETAILS || id == IDC_MATERIAL_HINT ||
                       id == IDC_MATERIAL_PREVIEW_HINT ||
                       id == IDC_MATERIAL_IMPORT_HINT;
    const int index = id == IDC_MATERIAL_TITLE           ? 2
                      : id == IDC_MATERIAL_PREVIEW_LABEL ? 1
                      : id == IDC_MATERIAL_PATH_LABEL    ? 1
                      : id == IDC_MATERIAL_STATUS        ? 1
                      : small                            ? 3
                                                         : 0;
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(fonts[index]),
                 TRUE);
  }
  for (HFONT font : material_fonts_)
    if (font) DeleteObject(font);
  material_fonts_ = fonts;
  SendDlgItemMessageW(materials_window_, IDC_MATERIAL_LIST, LB_SETITEMHEIGHT, 0,
                      settings_ui::Scale(materials_window_, 72));
}

void SettingsWindow::LoadMaterialControls() {
  if (!materials_window_) return;
  const bool busy = MaterialBusy();
  for (const int id :
       {IDC_MATERIAL_PATH, IDC_MATERIAL_BROWSE, IDC_MATERIAL_IMPORT,
        IDC_MATERIAL_REFRESH, IDC_MATERIAL_LIST, IDOK, IDCANCEL})
    EnableWindow(GetDlgItem(materials_window_, id), !busy);
  const HWND list = GetDlgItem(materials_window_, IDC_MATERIAL_LIST);
  const bool changed =
      SendMessageW(list, LB_GETCOUNT, 0, 0) == 0 ||
      material_list_entries_.size() != draft_.user_styles.size() ||
      !std::equal(material_list_entries_.begin(), material_list_entries_.end(),
                  draft_.user_styles.begin(),
                  [](const UserStyle& a, const UserStyle& b) {
                    return a.name == b.name &&
                           a.relative_path == b.relative_path;
                  });
  const auto top = SendMessageW(list, LB_GETTOPINDEX, 0, 0);
  if (changed) {
    SendMessageW(list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    material_list_entries_ = draft_.user_styles;
    if (material_list_entries_.empty())
      SendMessageW(list, LB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(Tr(L"尚未导入素材")));
    for (const auto& entry : material_list_entries_)
      SendMessageW(list, LB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(entry.name.c_str()));
    SendMessageW(list, LB_SETTOPINDEX, top, 0);
    SendMessageW(list, WM_SETREDRAW, TRUE, 0);
  }
  int selection = -1;
  std::wstring name;
  for (size_t i = 0; i < draft_.user_styles.size(); ++i) {
    if (draft_.is_custom() && draft_.user_styles[i].name == draft_.style_name) {
      name = draft_.user_styles[i].name;
      selection = static_cast<int>(i);
      break;
    }
  }
  EnableWindow(list, !busy && !draft_.user_styles.empty());
  const auto previous = SendMessageW(list, LB_GETCURSEL, 0, 0);
  SendMessageW(list, LB_SETCURSEL, selection, 0);
  if (previous == selection) SendMessageW(list, LB_SETTOPINDEX, top, 0);
  InvalidateRect(list, nullptr, FALSE);
  SetDlgItemTextW(materials_window_, IDC_MATERIAL_STATUS,
                  name.empty() ? Tr(L"未选择样式") : name.c_str());
  std::wstring details =
      Tr(L"尚未导入素材。\n支持 PNG、JPG、BMP、GIF 等格式。");
  std::wstring preview_details = Tr(L"导入后可在此预览");
  if (draft_.image_asset) {
    const auto& asset = *draft_.image_asset;
    details = (asset.is_gif ? Tr(L"GIF 动画") : Tr(L"静态图片")) +
              std::wstring(L"\n") + std::to_wstring(asset.width) + L" × " +
              std::to_wstring(asset.height) + Tr(L" 像素\n") +
              std::to_wstring(asset.frames.size()) + Tr(L" 帧");
    for (const auto& entry : draft_.user_styles)
      if (entry.name == draft_.style_name)
        details += L"\n" + entry.relative_path;
    preview_details = (name.empty() ? std::wstring(Tr(L"当前素材")) : name) +
                      L"\n" + std::to_wstring(asset.width) + L" × " +
                      std::to_wstring(asset.height) + L"  ·  " +
                      Tr(L"拖动图片 · 保存后生效");
  }
  SetDlgItemTextW(materials_window_, IDC_MATERIAL_DETAILS, details.c_str());
  SetDlgItemTextW(materials_window_, IDC_MATERIAL_PREVIEW_HINT,
                  preview_details.c_str());
  SetDlgItemTextW(materials_window_, IDC_MATERIAL_IMPORT_HINT,
                  busy
                      ? Tr(L"正在导入…")
                      : (draft_.image_asset ? Tr(L"已载入") : Tr(L"等待导入")));
  SetDlgItemTextW(materials_window_, IDC_MATERIAL_HINT,
                  Tr(L"素材会复制到 styles/，原文件可删除"));
  InvalidateRect(GetDlgItem(materials_window_, IDC_MATERIAL_PREVIEW), nullptr,
                 FALSE);
  InvalidateRect(materials_window_, nullptr, FALSE);
}

void SettingsWindow::LayoutMaterials() {
  if (!materials_window_) return;
  const RECT client = settings_ui::ClientDip(materials_window_);
  const auto place = [this](int id, int x, int y, int width, int height) {
    settings_ui::Place(materials_window_, id, x, y, width, height);
  };
  const int content_x = 26;
  const int content_right = client.right - 26;
  const int content_w = std::max(300, content_right - content_x);
  const int footer_top = client.bottom - 72;
  const int card_y = 82;
  const int preview_y = 250;
  const int preview_h = footer_top - 12 - preview_y;
  const int left_w = content_w * 62 / 100;
  const int right_x = content_x + left_w + 14;
  const int right_w = content_w - left_w - 14;

  place(IDC_MATERIAL_TITLE, content_x, 24, content_w - 260, 30);
  place(settings_controls::kLanguage, content_right - 132, 21, 132, 33);
  place(IDC_MATERIAL_PATH_LABEL, content_x + 16, card_y + 17, left_w - 160, 24);
  place(IDC_MATERIAL_PATH, content_x + 16, card_y + 45, left_w - 99, 31);
  place(IDC_MATERIAL_BROWSE, content_x + left_w - 76, card_y + 45, 60, 31);
  place(IDC_MATERIAL_FORMATS, content_x + 16, card_y + 99, left_w - 244, 20);
  place(IDC_MATERIAL_REFRESH, content_x + left_w - 188, card_y + 92, 66, 31);
  place(IDC_MATERIAL_IMPORT, content_x + left_w - 114, card_y + 92, 98, 31);
  place(IDC_MATERIAL_IMPORT_HINT, content_x + 16, card_y + 119, left_w - 244,
        18);

  place(IDC_MATERIAL_PREVIEW_LABEL, content_x + 16, preview_y + 16, 200, 24);
  place(IDC_MATERIAL_PREVIEW, content_x + 16, preview_y + 45, left_w - 32,
        std::max(80, preview_h - 105));
  place(IDC_MATERIAL_PREVIEW_HINT, content_x + 16, preview_y + preview_h - 52,
        left_w - 32, 40);

  place(IDC_MATERIAL_LIST, right_x + 16, card_y + 48, right_w - 32,
        std::max(72, footer_top - 88 - card_y - 48));

  place(IDC_MATERIAL_HINT, content_x, footer_top + 9, content_w - 250, 20);
  const bool english = draft_.ui_language == UiLanguage::kEnglish;
  const int cancel_width = english ? 70 : 56;
  const int save_width = english ? 100 : 82;
  place(IDCANCEL, content_right - cancel_width, footer_top + 7, cancel_width,
        33);
  place(IDOK, content_right - cancel_width - 8 - save_width, footer_top + 7,
        save_width, 33);
  InvalidateRect(materials_window_, nullptr, FALSE);
}

void SettingsWindow::PaintMaterials(HDC dc) {
  if (!materials_window_) return;
  RECT background{};
  GetClientRect(materials_window_, &background);
  settings_ui::Fill(dc, background, settings_ui::kBackground);
  const RECT client = settings_ui::ClientDip(materials_window_);
  const auto px = [this](int value) {
    return settings_ui::Scale(materials_window_, value);
  };
  const auto tag = [&](int x, int y, int width, const wchar_t* label) {
    RECT box{px(x), px(y), px(x + width), px(y + 19)};
    settings_ui::RoundedBox(materials_window_, dc, box,
                            settings_ui::kAccentTint,
                            settings_ui::kAccentTint, px(9));
    settings_ui::Text(dc, material_fonts_[3], label, box,
                      settings_ui::kAccent,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  };

  const int content_x = 26;
  const int content_right = client.right - 26;
  const int content_w = std::max(300, content_right - content_x);
  const int footer_top = client.bottom - 72;
  const int card_y = 82;
  const int import_h = 154;
  const int preview_y = 250;
  const int preview_h = footer_top - 12 - preview_y;
  const int left_w = content_w * 62 / 100;
  const int right_x = content_x + left_w + 14;
  const int right_w = content_w - left_w - 14;
  const int right_h = footer_top - 12 - card_y;

  settings_ui::Text(dc, material_fonts_[3],
                    Tr(L"把喜欢的图片或 GIF 变成提示样式"),
                    RECT{px(content_x), px(54), px(content_right), px(74)},
                    settings_ui::kMuted);
  const std::wstring count =
      Tr(L"已保存 ") + std::to_wstring(draft_.user_styles.size()) + Tr(L" 个");
  RECT count_box{px(content_right - 252), px(23), px(content_right - 142),
                 px(51)};
  settings_ui::RoundedBox(materials_window_, dc, count_box,
                          settings_ui::kPanel, settings_ui::kBorder, px(14));
  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  Gdiplus::SolidBrush dot(Gdiplus::Color(255, 34, 197, 94));
  graphics.FillEllipse(&dot, static_cast<float>(count_box.left + px(10)),
                       static_cast<float>(count_box.top + px(11)),
                       static_cast<float>(px(6)), static_cast<float>(px(6)));
  RECT count_text{count_box.left + px(22), count_box.top, count_box.right,
                  count_box.bottom};
  settings_ui::Text(dc, material_fonts_[3], count.c_str(), count_text,
                    settings_ui::kMuted,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);

  settings_ui::Panel(materials_window_, dc, content_x, card_y, left_w, import_h);
  settings_ui::Panel(materials_window_, dc, content_x, preview_y, left_w,
                     preview_h);
  settings_ui::Panel(materials_window_, dc, right_x, card_y, right_w, right_h);
  tag(content_x + left_w - 126, card_y + 14, 110, L"PNG / JPG / GIF");
  const int alpha_width = draft_.ui_language == UiLanguage::kEnglish ? 80 : 69;
  tag(content_x + left_w - 16 - alpha_width, preview_y + 14, alpha_width,
      Tr(L"透明背景"));

  settings_ui::Text(dc, material_fonts_[1], Tr(L"素材列表"),
                    RECT{px(right_x + 16), px(card_y + 16),
                         px(right_x + right_w - 16), px(card_y + 40)},
                    settings_ui::kText);
  if (draft_.user_styles.empty()) {
    settings_ui::Text(dc, material_fonts_[3], Tr(L"尚未导入素材"),
                      RECT{px(right_x + 16), px(card_y + 48),
                           px(right_x + right_w - 16), px(card_y + 96)},
                      settings_ui::kMuted,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }
  settings_ui::HorizontalRule(materials_window_, dc, px(right_x + 16),
                              px(right_x + right_w - 16), px(footer_top - 88));
  settings_ui::Text(dc, material_fonts_[3],
                    Tr(L"点击素材切换预览。\n保存配置后正式使用。"),
                    RECT{px(right_x + 16), px(footer_top - 80),
                         px(right_x + right_w - 16), px(footer_top - 38)},
                    settings_ui::kMuted, DT_LEFT | DT_TOP | DT_WORDBREAK);
  settings_ui::HorizontalRule(materials_window_, dc, px(content_x),
                              px(content_right), px(footer_top));
}

void SettingsWindow::DrawMaterialListItem(const DRAWITEMSTRUCT& item) {
  const settings_ui::PaintBuffer buffer(item.hDC, item.rcItem);
  const HDC dc = buffer.dc();
  const bool selected = (item.itemState & ODS_SELECTED) != 0;
  settings_ui::Fill(dc, item.rcItem,
                    selected ? settings_ui::kAccentTint : settings_ui::kPanel);
  if (item.itemID >= material_list_entries_.size()) {
    settings_ui::Text(dc, material_fonts_[3], Tr(L"尚未导入素材"), item.rcItem,
                      settings_ui::kMuted,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return;
  }
  const auto px = [&](int value) {
    return settings_ui::Scale(item.hwndItem, value);
  };
  const auto& entry = material_list_entries_[item.itemID];
  RECT thumb{item.rcItem.left, item.rcItem.top + px(12),
             item.rcItem.left + px(48), item.rcItem.top + px(60)};
  const auto asset = item.itemID < material_thumbnails_.size()
                         ? material_thumbnails_[item.itemID]
                         : nullptr;
  if (asset && asset->width && asset->height && !asset->frames.empty()) {
    Gdiplus::Graphics graphics(dc);
    Gdiplus::Bitmap image(
        asset->width, asset->height, asset->width * 4, PixelFormat32bppPARGB,
        const_cast<BYTE*>(asset->frames.front().pixels.data()));
    const float ratio =
        std::min(static_cast<float>(thumb.right - thumb.left) / asset->width,
                 static_cast<float>(thumb.bottom - thumb.top) / asset->height);
    const float width = asset->width * ratio;
    const float height = asset->height * ratio;
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.DrawImage(
        &image,
        Gdiplus::RectF((thumb.left + thumb.right - width) / 2,
                       (thumb.top + thumb.bottom - height) / 2, width, height));
  } else {
    settings_ui::Text(dc, material_fonts_[3], Tr(L"不可用"), thumb,
                      settings_ui::kMuted,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }
  RECT title{thumb.right + px(16), item.rcItem.top + px(12),
             item.rcItem.right - px(4), item.rcItem.top + px(36)};
  settings_ui::Text(dc, material_fonts_[0], entry.name.c_str(), title,
                    settings_ui::kText,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  RECT description{title.left, item.rcItem.top + px(36), title.right,
                   item.rcItem.top + px(60)};
  settings_ui::Text(dc, material_fonts_[3],
                    asset ? (asset->is_gif ? Tr(L"GIF 动画") : Tr(L"静态图片"))
                          : Tr(L"文件无法读取"),
                    description, settings_ui::kMuted,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  settings_ui::HorizontalRule(item.hwndItem, dc, item.rcItem.left,
                              item.rcItem.right, item.rcItem.bottom - 1);
  if ((item.itemState & ODS_FOCUS) && !(item.itemState & ODS_NOFOCUSRECT)) {
    RECT focus = item.rcItem;
    InflateRect(&focus, -px(2), -px(2));
    DrawFocusRect(dc, &focus);
  }
}

void SettingsWindow::DrawPreviewStage(HDC dc, HWND control,
                                      const RECT& bounds) {
  const auto px = [control](int value) {
    return settings_ui::Scale(control, value);
  };
  const int width = bounds.right - bounds.left;
  const int height = bounds.bottom - bounds.top;
  if (width <= 0 || height <= 0) return;
  settings_ui::Fill(dc, bounds, settings_ui::kBackground);
  settings_ui::RoundedBox(control, dc, bounds, settings_ui::kStage,
                          settings_ui::kStageBorder, px(18));

  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  Gdiplus::SolidBrush dot(Gdiplus::Color(28, 255, 255, 255));
  for (int y = px(18); y < height - px(18); y += px(24)) {
    for (int x = px(18); x < width - px(18); x += px(24)) {
      graphics.FillEllipse(&dot, static_cast<float>(x), static_cast<float>(y),
                           static_cast<float>(px(1.6)),
                           static_cast<float>(px(1.6)));
    }
  }

  RECT preview{px(17), px(13), px(89), px(35)};
  settings_ui::Text(dc, small_font_, Tr(L"实时预览"), preview,
                    settings_ui::kStageMuted,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT live{px(101), px(13), px(155), px(35)};
  settings_ui::RoundedBox(control, dc, live, settings_ui::kStageBorder,
                          settings_ui::kStageBorder, px(10));
  Gdiplus::SolidBrush live_dot(settings_ui::Color(settings_ui::kAccent));
  graphics.FillEllipse(&live_dot, static_cast<float>(px(109)),
                       static_cast<float>(px(22)), static_cast<float>(px(4)),
                       static_cast<float>(px(4)));
  RECT live_text{px(119), px(13), px(149), px(35)};
  settings_ui::Text(dc, small_font_, Tr(L"同步"), live_text,
                    settings_ui::kAccent, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  RECT hint{width - px(210), px(13), width - px(17), px(35)};
  settings_ui::Text(dc, small_font_,
                    draft_.is_custom() ? Tr(L"拖动图片 · 保存后生效")
                                       : Tr(L"点击穿透 · 不抢焦点"),
                    hint, settings_ui::kStageMuted,
                    DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

  const auto geometry = GetPreviewGeometry(control);
  const auto offset = draft_.active_image_offset();
  const float center_x = geometry.center_x;
  const float stage_top = static_cast<float>(px(45));
  const float stage_bottom = static_cast<float>(height - px(46));
  const float center_y = geometry.center_y;
  const float available_radius = std::max(
      1.0f,
      std::min(static_cast<float>(width - px(34)), stage_bottom - stage_top) /
          2.0f);
  Gdiplus::Pen orbit(settings_ui::Color(settings_ui::kStageMuted),
                     settings_ui::BorderWidth(control));
  orbit.SetDashStyle(Gdiplus::DashStyleDash);
  const float orbit_radius = available_radius;
  graphics.DrawEllipse(&orbit, center_x - orbit_radius, center_y - orbit_radius,
                       orbit_radius * 2, orbit_radius * 2);
  const float inner_radius = orbit_radius * 62.0f / 98.0f;
  graphics.DrawEllipse(&orbit, center_x - inner_radius, center_y - inner_radius,
                       inner_radius * 2, inner_radius * 2);

  const auto& style = draft_.active_style();
  // Fit the entire allowed range at a fixed scale, including half the widest
  // ring stroke (100 DIP radius + 20 / 2 DIP). Never clamp individual values:
  // every radius and thickness step must still change the preview geometry.
  const float radius_scale = geometry.scale;
  const float radius = style.radius_dip * radius_scale;
  const float thickness = style.thickness_dip * radius_scale;
  const BYTE alpha =
      static_cast<BYTE>(std::clamp(style.opacity_percent * 255 / 100, 0, 255));
  const Gdiplus::Color color(alpha, GetRValue(style.color),
                             GetGValue(style.color), GetBValue(style.color));

  if (draft_.is_custom() && draft_.image_asset &&
      !draft_.image_asset->frames.empty()) {
    const auto& asset = *draft_.image_asset;
    size_t index = 0;
    if (asset.is_gif && asset.duration_ms) {
      auto elapsed = (GetTickCount64() - preview_started_) % asset.duration_ms;
      while (index + 1 < asset.frames.size() &&
             elapsed >= asset.frames[index].delay_ms) {
        elapsed -= asset.frames[index].delay_ms;
        ++index;
      }
    }
    if (asset.width && asset.height) {
      const float extent = 2.0f * radius;
      const float image_width =
          asset.width * extent / std::max(asset.width, asset.height);
      const float image_height =
          asset.height * extent / std::max(asset.width, asset.height);
      Gdiplus::Bitmap image(
          asset.width, asset.height, asset.width * 4, PixelFormat32bppPARGB,
          const_cast<BYTE*>(asset.frames[index].pixels.data()));
      Gdiplus::ImageAttributes attributes;
      Gdiplus::ColorMatrix matrix = {
          {{1, 0, 0, 0, 0},
           {0, 1, 0, 0, 0},
           {0, 0, 1, 0, 0},
           {0, 0, 0, style.opacity_percent / 100.0f, 0},
           {0, 0, 0, 0, 1}}};
      attributes.SetColorMatrix(&matrix);
      graphics.SetInterpolationMode(
          Gdiplus::InterpolationModeHighQualityBicubic);
      const auto saved = graphics.Save();
      graphics.SetClip(Gdiplus::Rect(geometry.area.left, geometry.area.top,
                                     geometry.area.right - geometry.area.left,
                                     geometry.area.bottom - geometry.area.top));
      graphics.DrawImage(
          &image,
          Gdiplus::RectF(
              center_x + offset.x_dip * radius_scale - image_width / 2.0f,
              center_y + offset.y_dip * radius_scale - image_height / 2.0f,
              image_width, image_height),
          0.0f, 0.0f, static_cast<float>(asset.width),
          static_cast<float>(asset.height), Gdiplus::UnitPixel, &attributes);
      graphics.Restore(saved);
    }
  } else if (draft_.is_custom()) {
    RECT empty{0, static_cast<LONG>(center_y) + px(20), width,
               static_cast<LONG>(center_y) + px(42)};
    settings_ui::Text(dc, small_font_, Tr(L"导入后在此预览"), empty,
                      settings_ui::kStageMuted,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  } else if (draft_.mode == IndicatorMode::kHighlight) {
    Gdiplus::SolidBrush brush(color);
    graphics.FillEllipse(&brush, center_x - radius, center_y - radius,
                         radius * 2, radius * 2);
  } else {
    Gdiplus::Pen ring(color, thickness);
    graphics.DrawEllipse(&ring, center_x - radius, center_y - radius,
                         radius * 2, radius * 2);
  }

  graphics.Flush(Gdiplus::FlushIntentionSync);
  DrawPreviewCursor(dc, control);

  const std::wstring mode =
      draft_.is_custom()
          ? (draft_.image_asset && draft_.image_asset->is_gif ? L"GIF"
                                                              : Tr(L"图片"))
          : (draft_.mode == IndicatorMode::kHighlight ? Tr(L"高亮")
                                                      : Tr(L"光圈"));
  wchar_t color_text[16]{};
  std::swprintf(color_text, 16, L"#%02X%02X%02X",
                static_cast<unsigned int>(GetRValue(style.color)),
                static_cast<unsigned int>(GetGValue(style.color)),
                static_cast<unsigned int>(GetBValue(style.color)));
  const std::wstring radius_text = std::to_wstring(style.radius_dip) + L" DIP";
  const std::wstring hold_text =
      std::to_wstring(draft_.hold_seconds) + Tr(L" 秒后隐藏");
  const wchar_t* labels[] = {mode.c_str(), radius_text.c_str(),
                             hold_text.c_str(), color_text};
  const bool english = draft_.ui_language == UiLanguage::kEnglish;
  const int widths[] = {english ? 70 : 48, english ? 70 : 62,
                        english ? 120 : 78, 76};
  int chip_x = px(17);
  const int chip_y = height - px(38);
  for (int i = 0; i < 4; ++i) {
    RECT chip{chip_x, chip_y, chip_x + px(widths[i]), chip_y + px(24)};
    settings_ui::RoundedBox(control, dc, chip, settings_ui::kStageBorder,
                            settings_ui::kStageBorder, px(6));
    settings_ui::Text(dc, i == 3 ? number_font_ : small_font_, labels[i], chip,
                      settings_ui::kStageMuted,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    chip_x += px(widths[i] + 6);
  }
}

void SettingsWindow::DrawMaterialPreview(const DRAWITEMSTRUCT& item) {
  const bool material = item.CtlID == IDC_MATERIAL_PREVIEW;
  const int width = item.rcItem.right - item.rcItem.left;
  const int height = item.rcItem.bottom - item.rcItem.top;
  if (width <= 0 || height <= 0) return;
  HDC dc = CreateCompatibleDC(item.hDC);
  HBITMAP bitmap = CreateCompatibleBitmap(item.hDC, width, height);
  if (!dc || !bitmap) {
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
    return;
  }
  const auto old_bitmap = SelectObject(dc, bitmap);
  const auto paint = [&]() {
    const RECT bounds{0, 0, width, height};
    if (!material) {
      DrawPreviewStage(dc, item.hwndItem, bounds);
      return;
    }
    SetDCBrushColor(dc, settings_ui::kPanel);
    FillRect(dc, &bounds, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    const int saved_dc = SaveDC(dc);
    const int corner = settings_ui::Scale(item.hwndItem, 10);
    HRGN clip =
        CreateRoundRectRgn(0, 0, width + 1, height + 1, corner * 2, corner * 2);
    if (clip) {
      SelectClipRgn(dc, clip);
      DeleteObject(clip);
    }
    {
      Gdiplus::Graphics graphics(dc);
      Gdiplus::SolidBrush light(settings_ui::Color(settings_ui::kPanel));
      Gdiplus::SolidBrush dark(settings_ui::Color(settings_ui::kBorder));
      const int cell = settings_ui::Scale(item.hwndItem, 16);
      if (material || draft_.is_custom()) {
        for (int y = 0; y < height; y += cell) {
          for (int x = 0; x < width; x += cell) {
            graphics.FillRectangle(((x / cell + y / cell) % 2) ? &dark : &light,
                                   x, y, std::min(cell, width - x),
                                   std::min(cell, height - y));
          }
        }
      } else {
        Gdiplus::SolidBrush background(settings_ui::Color(
            draft_.ui_theme == UiTheme::kMinimal ? settings_ui::kAccentTint
                                                 : settings_ui::kBackground));
        graphics.FillRectangle(&background, 0, 0, width, height);
        Gdiplus::Pen grid(settings_ui::Color(settings_ui::kBorder));
        graphics.DrawLine(&grid, width / 2, 0, width / 2, height);
        graphics.DrawLine(&grid, 0, height / 2, width, height / 2);
      }
      const bool custom = material || draft_.is_custom();
      if (custom && draft_.image_asset && !draft_.image_asset->frames.empty()) {
        const auto& asset = *draft_.image_asset;
        size_t index = 0;
        if (asset.is_gif && asset.duration_ms) {
          auto elapsed =
              (GetTickCount64() - preview_started_) % asset.duration_ms;
          while (index + 1 < asset.frames.size() &&
                 elapsed >= asset.frames[index].delay_ms) {
            elapsed -= asset.frames[index].delay_ms;
            ++index;
          }
        }
        if (asset.width && asset.height) {
          const auto geometry = GetPreviewGeometry(item.hwndItem);
          const auto offset = draft_.active_image_offset();
          const float ratio = 2.0f * draft_.custom.radius_dip * geometry.scale /
                              std::max(asset.width, asset.height);
          const float image_width = asset.width * ratio;
          const float image_height = asset.height * ratio;
          Gdiplus::Bitmap image(
              asset.width, asset.height, asset.width * 4, PixelFormat32bppPARGB,
              const_cast<BYTE*>(asset.frames[index].pixels.data()));
          Gdiplus::ImageAttributes attributes;
          Gdiplus::ColorMatrix matrix = {
              {{1, 0, 0, 0, 0},
               {0, 1, 0, 0, 0},
               {0, 0, 1, 0, 0},
               {0, 0, 0, draft_.custom.opacity_percent / 100.0f, 0},
               {0, 0, 0, 0, 1}}};
          attributes.SetColorMatrix(&matrix);
          graphics.SetInterpolationMode(
              Gdiplus::InterpolationModeHighQualityBicubic);
          graphics.SetClip(
              Gdiplus::Rect(geometry.area.left, geometry.area.top,
                            geometry.area.right - geometry.area.left,
                            geometry.area.bottom - geometry.area.top));
          const float image_x =
              geometry.center_x + offset.x_dip * geometry.scale;
          const float image_y =
              geometry.center_y + offset.y_dip * geometry.scale;
          graphics.DrawImage(&image,
                             Gdiplus::RectF(image_x - image_width / 2,
                                            image_y - image_height / 2,
                                            image_width, image_height),
                             0.0f, 0.0f, static_cast<float>(asset.width),
                             static_cast<float>(asset.height),
                             Gdiplus::UnitPixel, &attributes);
        }
      } else if (!custom) {
        const auto& style = draft_.active_style();
        const float scale = GetDpiForWindow(item.hwndItem) / 96.0f;
        const float fit = std::min(
            1.0f,
            (std::min(width, height) - 24.0f * scale) /
                ((2.0f * style.radius_dip + style.thickness_dip) * scale));
        const float radius = std::max(1.0f, style.radius_dip * scale * fit);
        const Gdiplus::RectF ellipse(width / 2.0f - radius,
                                     height / 2.0f - radius, radius * 2,
                                     radius * 2);
        const Gdiplus::Color color(
            static_cast<BYTE>(style.opacity_percent * 255 / 100),
            GetRValue(style.color), GetGValue(style.color),
            GetBValue(style.color));
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        if (draft_.mode == IndicatorMode::kHighlight) {
          Gdiplus::SolidBrush brush(color);
          graphics.FillEllipse(&brush, ellipse);
        } else {
          Gdiplus::Pen pen(color, style.thickness_dip * scale * fit);
          graphics.DrawEllipse(&pen, ellipse);
        }
      }
    }
    DrawPreviewCursor(dc, item.hwndItem);
    if (!draft_.image_asset) {
      RECT empty = bounds;
      empty.top = height / 2 + settings_ui::Scale(item.hwndItem, 20);
      settings_ui::Text(dc, material ? material_fonts_[3] : small_font_,
                        Tr(L"导入后在此预览"), empty, settings_ui::kMuted,
                        DT_CENTER | DT_TOP | DT_SINGLELINE);
    }
    RestoreDC(dc, saved_dc);
    settings_ui::RoundedBox(item.hwndItem, dc, bounds, settings_ui::kPanel,
                            settings_ui::kBorder, corner, false);
  };
  paint();
  BitBlt(item.hDC, item.rcItem.left, item.rcItem.top, width, height, dc, 0, 0,
         SRCCOPY);
  SelectObject(dc, old_bitmap);
  DeleteObject(bitmap);
  DeleteDC(dc);
}
}  // namespace mouse_cursor_detector
