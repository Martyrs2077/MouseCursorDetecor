#include "mouse_cursor_detector/settings/settings_window.h"

#include <algorithm>
#include <string>

#include "mouse_cursor_detector/resources/resource.h"
#include "mouse_cursor_detector/settings/settings_control_frames.h"
#include "mouse_cursor_detector/settings/settings_control_ids.h"
#include "mouse_cursor_detector/settings/settings_ui.h"

namespace mouse_cursor_detector {
namespace {
namespace ids = settings_controls;
const wchar_t* ThemeName(UiTheme theme) {
  switch (theme) {
    case UiTheme::kNative:
      return Tr(L"清晰原生");
    case UiTheme::kDark:
      return Tr(L"暗色工作台");
    case UiTheme::kPaper:
      return Tr(L"纸感仪表");
    default:
      return Tr(L"极简聚焦");
  }
}

const wchar_t* ThemeCaption(UiTheme theme) {
  switch (theme) {
    case UiTheme::kNative:
      return Tr(L"云杉绿 · 清晰直接");
    case UiTheme::kDark:
      return Tr(L"石墨铜 · 低光专注");
    case UiTheme::kPaper:
      return Tr(L"纸莎棕 · 温暖手感");
    default:
      return Tr(L"石墨蓝 · 居中聚焦");
  }
}

void DrawBrandMark(HDC dc, HWND window) {
  const auto px = [window](int value) {
    return settings_ui::Scale(window, value);
  };
  // Shared resource handles are cached by Windows and must not be destroyed.
  const auto icon = static_cast<HICON>(
      LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON),
                 IMAGE_ICON, px(38), px(38), LR_SHARED));
  if (icon)
    DrawIconEx(dc, px(19), px(22), icon, px(38), px(38), 0, nullptr, DI_NORMAL);
}

void ClipToWorkspace(HWND window, HWND control) {
  if (!control) return;
  RECT bounds{};
  if (!GetWindowRect(control, &bounds)) return;
  MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&bounds), 2);
  const int width = bounds.right - bounds.left;
  const int height = bounds.bottom - bounds.top;
  const int left = std::clamp(
      settings_ui::Scale(window, 164) - static_cast<int>(bounds.left), 0,
      width);
  RECT current{};
  const int type = GetWindowRgnBox(control, &current);
  if (left == 0) {
    if (type != ERROR) SetWindowRgn(control, nullptr, FALSE);
    return;
  }
  const RECT clip{left, 0, width, height};
  if (type != ERROR && EqualRect(&clip, &current)) return;
  const HRGN region = CreateRectRgn(left, 0, width, height);
  // A successful SetWindowRgn transfers ownership to Windows.
  if (region && !SetWindowRgn(control, region, FALSE)) DeleteObject(region);
}

void DrawTag(HDC dc, HWND window, int x, int y, int width,
             const wchar_t* label, HFONT font) {
  const RECT box{settings_ui::Scale(window, x), settings_ui::Scale(window, y),
                 settings_ui::Scale(window, x + width),
                 settings_ui::Scale(window, y + 19)};
  settings_ui::RoundedBox(window, dc, box, settings_ui::kAccentTint,
                          settings_ui::kAccentTint, settings_ui::Scale(window, 9));
  settings_ui::Text(dc, font, label, box, settings_ui::kAccent,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawRailNote(HDC dc, HWND window, HFONT title_font, HFONT font) {
  const RECT note{settings_ui::Scale(window, 14),
                  settings_ui::Scale(window, 590),
                  settings_ui::Scale(window, 150),
                  settings_ui::Scale(window, 652)};
  settings_ui::RoundedBox(window, dc, note, settings_ui::kPanelAlt,
                          settings_ui::kBorder, settings_ui::Scale(window, 10));
  RECT text{note.left + settings_ui::Scale(window, 10),
            note.top + settings_ui::Scale(window, 6),
            note.right - settings_ui::Scale(window, 8),
            note.top + settings_ui::Scale(window, 24)};
  settings_ui::Text(dc, title_font, Tr(L"原光标不变"), text, settings_ui::kText,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  text.top += settings_ui::Scale(window, 16);
  text.bottom += settings_ui::Scale(window, 16);
  settings_ui::Text(dc, font, Tr(L"只叠加提示层，"), text, settings_ui::kMuted,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  text.top += settings_ui::Scale(window, 13);
  text.bottom += settings_ui::Scale(window, 13);
  settings_ui::Text(dc, font, Tr(L"不替换系统指针。"), text,
                    settings_ui::kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void DrawTopStatus(HDC dc, HWND window, HFONT font, int right, int width,
                   const wchar_t* label) {
  const auto px = [window](int value) {
    return settings_ui::Scale(window, value);
  };
  const int status_x = right - width;
  const int top = px(23);
  RECT chip{status_x, top, status_x + width, top + px(28)};
  settings_ui::RoundedBox(window, dc, chip, settings_ui::kPanel,
                          settings_ui::kBorder, px(14));
  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  Gdiplus::SolidBrush dot(Gdiplus::Color(255, 34, 197, 94));
  graphics.FillEllipse(&dot, static_cast<float>(chip.left + px(10)),
                       static_cast<float>(chip.top + px(11)),
                       static_cast<float>(px(6)), static_cast<float>(px(6)));
  RECT text{chip.left + px(22), chip.top, chip.right - px(8), chip.bottom};
  settings_ui::Text(dc, font, label, text, settings_ui::kMuted,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}
}  // namespace

void SettingsWindow::ApplyTheme() {
  const bool changed = !theme_applied_ || applied_theme_ != draft_.ui_theme;
  // All four themes share the same layout. A theme update only changes paint.
  const settings_ui::ScopedRedraw redraw(changed ? window_ : nullptr);
  settings_ui::UseTheme(draft_.ui_theme);
  if (changed) {
    for (const int id :
         {ids::kRadius, ids::kThickness, ids::kOpacity, ids::kHoldSeconds})
      SendDlgItemMessageW(window_, id, WM_SETFONT,
                          reinterpret_cast<WPARAM>(font_), FALSE);
    for (const int id : {ids::kRadius, ids::kThickness, ids::kOpacity,
                         ids::kHoldSeconds, ids::kHotkey}) {
      settings_ui::PrepareField(GetDlgItem(window_, id), settings_ui::kPanel);
    }
    settings_ui::PrepareCombo(GetDlgItem(window_, ids::kMode),
                              settings_ui::kPanel);
  }
  const std::wstring state =
      draft_.ui_theme == original_.ui_theme
          ? std::wstring(Tr(L"已保存：")) + ThemeName(original_.ui_theme)
          : std::wstring(Tr(L"尚未保存 · 上次保存：")) +
                ThemeName(original_.ui_theme);
  settings_ui::SetControlText(window_, ids::kThemeState, state.c_str());
  for (int i = 0; i < 4; ++i) {
    const auto theme = static_cast<UiTheme>(i);
    const auto label = std::wstring(ThemeName(theme)) +
                       (draft_.ui_theme == theme ? Tr(L"（已选择）") : L"");
    settings_ui::SetControlText(window_, ids::kThemeFirst + i, label.c_str());
  }
  applied_theme_ = draft_.ui_theme;
  theme_applied_ = true;
  if (changed)
    RedrawWindow(window_, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
}

void SettingsWindow::SelectPage(Page page) {
  if (page == page_) {
    if (page == Page::kMaterials) BeginLibraryRefresh();
    return;
  }
  if (naming_style_ || MaterialBusy()) {
    MessageBoxW(window_, Tr(L"正在处理素材，请完成后再切换页面。"),
                Tr(L"素材导入"), MB_OK | MB_ICONINFORMATION);
    return;
  }
  if (page_ == Page::kMain && !ReadControls(true)) return;
  if (materials_window_) {
    CloseMaterials(false, page);
    return;
  }
  if (page == Page::kMaterials) {
    ShowMaterials();
    return;
  }
  {
    const settings_ui::ScopedRedraw redraw(window_);
    page_ = page;
    scroll_dip_ = 0;
    horizontal_scroll_dip_ = 0;
    LayoutControls();
  }
  SetFocus(GetDlgItem(window_,
                      page == Page::kThemes ? ids::kThemesTab : ids::kMainTab));
}

void SettingsWindow::LayoutControls() {
  if (layout_active_) return;
  RECT client = settings_ui::ClientDip(window_);
  if (client.right < 100 || client.bottom < 100) return;
  const settings_ui::ScopedRedraw redraw(window_);
  layout_active_ = true;
  for (int pass = 0; pass < 2; ++pass) {
    client = settings_ui::ClientDip(window_);
    const auto update_scroll = [this](int bar, int minimum, int visible,
                                      int* position) {
      *position = std::clamp(*position, 0, std::max(0, minimum - visible));
      SCROLLINFO scroll{};
      scroll.cbSize = sizeof(scroll);
      scroll.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
      scroll.nMax = std::max(minimum, visible) - 1;
      scroll.nPage = visible;
      scroll.nPos = *position;
      SetScrollInfo(window_, bar, &scroll, TRUE);
    };
    update_scroll(SB_VERT, settings_ui::kMinClientHeight, client.bottom,
                  &scroll_dip_);
    client = settings_ui::ClientDip(window_);
    update_scroll(SB_HORZ, settings_ui::kMinClientWidth, client.right,
                  &horizontal_scroll_dip_);
  }
  client = settings_ui::ClientDip(window_);
  client.right = std::max<LONG>(settings_ui::kMinClientWidth, client.right);
  client.bottom = std::max<LONG>(settings_ui::kMinClientHeight, client.bottom);
  const auto place = [this](int id, int x, int y, int width, int height) {
    settings_ui::Place(window_, id, x - horizontal_scroll_dip_, y - scroll_dip_,
                       width, height);
    ClipToWorkspace(window_, GetDlgItem(window_, id));
  };
  const bool main = page_ == Page::kMain;
  const bool themes = page_ == Page::kThemes;
  const bool materials = page_ == Page::kMaterials;
  const bool ring = draft_.mode == IndicatorMode::kRing;
  const int content_x = 190;
  const int content_right = client.right - 26;
  const int content_w = std::max(300, content_right - content_x);
  const int footer_top = client.bottom - 72;
  const int stage_y = 82;
  const int stage_h = 232;
  const int grid_y = stage_y + stage_h + 12;
  const int card_h = std::max(310, footer_top - 12 - grid_y);
  const int left_w = content_w * 53 / 100;
  const int right_x = content_x + left_w + 14;
  const int right_w = content_w - left_w - 14;

  for (int i = 0; i < 3; ++i) {
    settings_ui::Place(window_, ids::kMainTab + i, 14, 84 + i * 46, 136, 42);
    InvalidateRect(GetDlgItem(window_, ids::kMainTab + i), nullptr, FALSE);
  }
  const int title_reserve =
      themes && draft_.ui_language == UiLanguage::kEnglish ? 346 : 240;
  place(ids::kTitle, content_x, 24, content_w - title_reserve, 30);
  place(ids::kLanguage, content_right - 132, 21, 132, 33);
  settings_ui::SetControlText(
      window_, ids::kTitle,
      themes ? Tr(L"外观主题") : (main ? Tr(L"提示设置") : Tr(L"素材库")));

  for (HWND child = GetWindow(window_, GW_CHILD); child;
       child = GetWindow(child, GW_HWNDNEXT)) {
    if (child == materials_window_) continue;
    const int id = GetDlgCtrlID(child);
    bool visible = false;
    if (id == ids::kLanguage) {
      visible = !materials;
    } else if (id >= ids::kMainTab && id <= ids::kThemesTab) {
      visible = true;
    } else if (materials) {
      visible = false;
    } else if (themes) {
      visible = id == ids::kTitle || id == ids::kHint ||
                id == ids::kReset || id == IDOK || id == IDCANCEL ||
                (id >= ids::kThemeFirst && id <= ids::kThemeLast);
    } else if (main) {
      switch (id) {
        case ids::kTitle:
        case ids::kPreview:
        case 230:
        case ids::kRingChoice:
        case ids::kHighlightChoice:
        case 231:
        case ids::kMode:
        case 233:
        case ids::kColor:
        case ids::kRadiusLabel:
        case ids::kRadius:
        case ids::kRadiusSlider:
        case 232:
        case ids::kThicknessLabel:
        case ids::kThickness:
        case ids::kThicknessUnit:
        case ids::kThicknessSlider:
        case ids::kOpacityLabel:
        case ids::kOpacity:
        case ids::kOpacitySlider:
        case 235:
        case ids::kHotkey:
        case ids::kChangeHotkey:
        case 239:
        case 236:
        case ids::kHoldSeconds:
        case 237:
        case 254:
        case ids::kStartup:
        case ids::kStartupHint:
        case 242:
        case ids::kHint:
        case ids::kReset:
        case IDOK:
        case IDCANCEL:
          visible = true;
          break;
        default:
          break;
      }
      if (!ring) {
        if (id == ids::kThickness || id == ids::kThicknessLabel ||
            id == ids::kThicknessUnit || id == ids::kThicknessSlider)
          visible = false;
      }
    }
    settings_ui::ShowControl(child, visible);
  }

  if (materials) {
    if (materials_window_) {
      settings_ui::PlaceWindow(window_, materials_window_,
                               164 - horizontal_scroll_dip_, -scroll_dip_,
                               client.right - 164, client.bottom);
      ClipToWorkspace(window_, materials_window_);
      settings_ui::ShowControl(materials_window_, true);
      LayoutMaterials();
    }
    layout_active_ = false;
    InvalidateRect(window_, nullptr, FALSE);
    return;
  }

  if (themes) {
    const int gap = 14;
    const int card_w = (content_w - gap) / 2;
    const int theme_top = 82;
    const int grid_bottom = footer_top - 12;
    const int card_h_theme = (grid_bottom - theme_top - gap) / 2;
    const int order[] = {3, 0, 1, 2};
    for (int i = 0; i < 4; ++i) {
      place(ids::kThemeFirst + order[i], content_x + (i % 2) * (card_w + gap),
            theme_top + (i / 2) * (card_h_theme + gap), card_w, card_h_theme);
    }
    LayoutFooter(content_x, content_right, footer_top);
    layout_active_ = false;
    InvalidateRect(window_, nullptr, FALSE);
    return;
  }

  place(ids::kPreview, content_x, stage_y, content_w, stage_h);
  const int ax = content_x;
  const int ay = grid_y;
  const int aw = left_w;
  const int seg_w = aw - 32;
  const int seg_half = (seg_w - 3) / 2;
  place(230, ax + 16, ay + 16, 140, 24);
  place(ids::kRingChoice, ax + 16, ay + 50, seg_half, 35);
  place(ids::kHighlightChoice, ax + 19 + seg_half, ay + 50, seg_half, 35);
  place(231, ax + 16, ay + 99, 54, 30);
  place(ids::kMode, ax + 74, ay + 99, aw - 90, 30);
  place(233, ax + 16, ay + 137, 54, 30);
  place(ids::kColor, ax + 74, ay + 137, aw - 90, 30);

  const auto parameter_row = [&](int row, int label_id, int slider_id,
                                 int edit_id, int unit_id) {
    const int y = ay + 177 + row * 34;
    place(label_id, ax + 16, y + 3, 56, 24);
    place(slider_id, ax + 80, y, aw - 182, 28);
    place(edit_id, ax + aw - 88, y, 56, 31);
    place(unit_id, ax + aw - 30, y + 3, 20, 20);
  };
  parameter_row(0, ids::kRadiusLabel, ids::kRadiusSlider, ids::kRadius, 232);
  parameter_row(1, ids::kThicknessLabel, ids::kThicknessSlider,
                ids::kThickness, ids::kThicknessUnit);
  parameter_row(2, ids::kOpacityLabel, ids::kOpacitySlider, ids::kOpacity, 235);

  const int tx = right_x;
  const int ty = grid_y;
  const int tw = right_w;
  place(239, tx + 16, ty + 16, 120, 24);
  place(ids::kHotkey, tx + 16, ty + 50, tw - 100, 31);
  place(ids::kChangeHotkey, tx + tw - 80, ty + 50, 64, 31);
  place(236, tx + 16, ty + 115, 96, 24);
  place(ids::kHoldSeconds, tx + tw - 102, ty + 111, 56, 31);
  place(237, tx + tw - 42, ty + 116, 22, 20);
  place(254, tx + 16, ty + 150, tw - 32, 20);

  const int sy = grid_y + 196;
  place(242, tx + 16, sy + 40, 120, 24);
  place(ids::kStartup, tx + tw - 52, sy + 38, 36, 21);
  place(ids::kStartupHint, tx + 16, sy + 66, tw - 80, 20);

  LayoutFooter(content_x, content_right, footer_top);
  layout_active_ = false;
  InvalidateRect(window_, nullptr, FALSE);
}

void SettingsWindow::LayoutFooter(int left, int right, int top) {
  const bool english = draft_.ui_language == UiLanguage::kEnglish;
  const int ok_w = english ? 112 : 82;
  const int cancel_w = english ? 70 : 56;
  const int reset_w = english ? 78 : 68;
  const int ok_x = right - ok_w;
  const int cancel_x = ok_x - 8 - cancel_w;
  const int reset_x = cancel_x - 8 - reset_w;
  settings_ui::Place(window_, ids::kHint, left, top + 9,
                     std::max(80, reset_x - left - 16), 20);
  settings_ui::Place(window_, ids::kReset, reset_x, top + 7, reset_w, 33);
  settings_ui::Place(window_, IDCANCEL, cancel_x, top + 7, cancel_w, 33);
  settings_ui::Place(window_, IDOK, ok_x, top + 7, ok_w, 33);
}

void SettingsWindow::PaintWindow(HDC dc) {
  RECT background{};
  GetClientRect(window_, &background);
  settings_ui::Fill(dc, background, settings_ui::kBackground);
  RECT client = settings_ui::ClientDip(window_);
  client.right = std::max<LONG>(settings_ui::kMinClientWidth, client.right);
  client.bottom = std::max<LONG>(settings_ui::kMinClientHeight, client.bottom);

  const auto px = [this](int value) {
    return settings_ui::Scale(window_, value);
  };
  const RECT rail{0, 0, px(164), px(client.bottom)};
  settings_ui::Fill(dc, rail, settings_ui::kRail);
  RECT rail_line{px(163), 0, px(164), px(client.bottom)};
  settings_ui::Fill(dc, rail_line, settings_ui::kBorder);
  DrawBrandMark(dc, window_);
  settings_ui::Text(dc, heading_font_, Tr(L"鼠标提示"),
                    RECT{px(67), px(28), px(150), px(54)}, settings_ui::kText);
  settings_ui::Text(dc, small_font_, L"MOUSE CURSOR",
                    RECT{px(19), px(62), px(150), px(80)}, settings_ui::kMuted);
  DrawRailNote(dc, window_, heading_font_, small_font_);
  settings_ui::Text(dc, small_font_, L"v0.1.0",
                    RECT{px(18), px(670), px(70), px(690)},
                    settings_ui::kMuted);
  settings_ui::Text(dc, small_font_, L"WIN32",
                    RECT{px(112), px(670), px(148), px(690)},
                    settings_ui::kMuted);

  if (page_ == Page::kMaterials) return;

  const int saved = SaveDC(dc);
  IntersectClipRect(dc, px(164), 0, background.right, background.bottom);
  SetViewportOrgEx(dc, -px(horizontal_scroll_dip_), -px(scroll_dip_), nullptr);
  const int content_x = 190;
  const int content_right = client.right - 26;
  const int content_w = std::max(300, content_right - content_x);
  const int footer_top = client.bottom - 72;

  settings_ui::Text(dc, small_font_,
                    page_ == Page::kThemes ? Tr(L"四种气质，一套操作")
                                           : Tr(L"让指针在屏幕上被看见"),
                    RECT{px(content_x), px(54), px(content_right), px(74)},
                    settings_ui::kMuted);
  const int status_width =
      page_ == Page::kThemes
          ? (draft_.ui_language == UiLanguage::kEnglish ? 190 : 112)
          : 74;
  std::wstring status_text;
  if (page_ == Page::kThemes) {
    status_text = std::wstring(Tr(L"当前：")) + ThemeName(draft_.ui_theme);
  } else {
    status_text = Tr(L"运行中");
  }
  DrawTopStatus(dc, window_, small_font_, px(content_right - 142),
                px(status_width), status_text.c_str());

  if (page_ == Page::kMain) {
    const int grid_y = 326;
    const int left_w = content_w * 53 / 100;
    const int right_x = content_x + left_w + 14;
    const int right_w = content_w - left_w - 14;
    const int card_h = std::max(310, footer_top - 12 - grid_y);
    const int right_y = grid_y + 196;

    settings_ui::Panel(window_, dc, content_x, grid_y, left_w, card_h);
    settings_ui::Panel(window_, dc, right_x, grid_y, right_w, 184);
    settings_ui::Panel(window_, dc, right_x, right_y, right_w,
                       std::max(114, card_h - 196));
    DrawTag(dc, window_, content_x + left_w - 70, grid_y + 16, 54,
            Tr(L"实时应用"), small_font_);
    DrawTag(dc, window_, right_x + right_w - 78, grid_y + 16, 64,
            Tr(L"全局监听"), small_font_);

    RECT system_title{px(right_x + 16), px(right_y + 16), px(right_x + right_w),
                      px(right_y + 40)};
    settings_ui::Text(dc, heading_font_, Tr(L"系统"), system_title,
                      settings_ui::kText);
    RECT system_note{px(right_x + 16), px(right_y + 94),
                     px(right_x + right_w - 16), px(right_y + 112)};
    settings_ui::Text(dc, small_font_, Tr(L"提示显示状态仅本次运行有效"),
                      system_note, settings_ui::kMuted);
    RECT trigger_rule{px(right_x + 16), px(grid_y + 96),
                      px(right_x + right_w - 16), px(grid_y + 97)};
    settings_ui::Fill(dc, trigger_rule, settings_ui::kBorder);
  }

  settings_ui::HorizontalRule(window_, dc, px(content_x), px(content_right),
                              px(footer_top));
  RestoreDC(dc, saved);
}

void SettingsWindow::DrawThemeCard(const DRAWITEMSTRUCT& item) {
  const auto theme = static_cast<UiTheme>(item.CtlID - ids::kThemeFirst);
  const auto palette = settings_ui::ThemePalette(theme);
  const bool selected = draft_.ui_theme == theme;
  const auto px = [this](int value) {
    return settings_ui::Scale(window_, value);
  };
  const RECT bounds = item.rcItem;
  settings_ui::Fill(item.hDC, bounds, settings_ui::kBackground);
  settings_ui::RoundedBox(
      item.hwndItem, item.hDC, bounds, palette.panel,
      selected ? palette.accent : palette.border, px(16));

  RECT preview{bounds.left + px(9), bounds.top + px(9),
               bounds.right - px(9), bounds.bottom - px(66)};
  settings_ui::RoundedBox(item.hwndItem, item.hDC, preview, palette.background,
                          palette.border, px(10));

  const int preview_w = preview.right - preview.left;
  const int rail_w = std::min(px(55), preview_w / 4);
  RECT rail{preview.left + px(1), preview.top + px(1),
            preview.left + rail_w, preview.bottom - px(1)};
  settings_ui::Fill(item.hDC, rail, palette.rail);
  RECT rail_line{rail.right - px(1), preview.top + px(1), rail.right,
                 preview.bottom - px(1)};
  settings_ui::Fill(item.hDC, rail_line, palette.border);

  Gdiplus::Graphics graphics(item.hDC);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  Gdiplus::SolidBrush accent(settings_ui::Color(palette.accent));
  graphics.FillRectangle(&accent, static_cast<float>(preview.left + px(10)),
                         static_cast<float>(preview.top + px(10)),
                         static_cast<float>(px(13)), static_cast<float>(px(13)));
  for (int i = 0; i < 3; ++i) {
    RECT nav{preview.left + px(8), preview.top + px(34 + i * 14),
             rail.right - px(8), preview.top + px(41 + i * 14)};
    settings_ui::RoundedBox(item.hwndItem, item.hDC, nav,
                            i == 0 ? palette.panel : palette.border,
                            i == 0 ? palette.panel : palette.border, px(2));
  }
  const int main_left = preview.left + rail_w + px(10);
  const int main_right = preview.right - px(10);
  const int main_width = std::max(1, main_right - main_left);
  RECT mini_stage{main_left, preview.top + px(32), main_right,
                  preview.bottom - px(42)};
  settings_ui::RoundedBox(item.hwndItem, item.hDC, mini_stage, palette.stage,
                          palette.stage_border, px(6));
  const int center_x = (mini_stage.left + mini_stage.right) / 2;
  const int center_y = (mini_stage.top + mini_stage.bottom) / 2;
  Gdiplus::Pen ring(settings_ui::Color(palette.accent), px(3));
  graphics.DrawEllipse(&ring, center_x - px(19), center_y - px(19), px(38),
                       px(38));
  RECT title_bar{main_left, preview.top + px(10), main_right,
                 preview.top + px(22)};
  settings_ui::Text(item.hDC, small_font_, Tr(L"鼠标提示"), title_bar,
                    palette.text);
  const int mini_card_w = std::max(1, (main_width - px(6)) / 2);
  for (int i = 0; i < 2; ++i) {
    RECT card{main_left + i * (mini_card_w + px(6)), preview.bottom - px(32),
              main_left + i * (mini_card_w + px(6)) + mini_card_w,
              preview.bottom - px(8)};
    settings_ui::RoundedBox(item.hwndItem, item.hDC, card, palette.panel,
                            palette.border, px(4));
  }

  if (selected) {
    RECT tag{preview.right - px(49), preview.top + px(7),
             preview.right - px(8), preview.top + px(24)};
    settings_ui::RoundedBox(item.hwndItem, item.hDC, tag, palette.accent,
                            palette.accent, px(8));
    settings_ui::Text(item.hDC, small_font_, Tr(L"当前"), tag,
                      palette.on_accent,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }

  RECT name{bounds.left + px(12), bounds.bottom - px(48),
            bounds.right - px(34), bounds.bottom - px(28)};
  settings_ui::Text(item.hDC, heading_font_, ThemeName(theme), name,
                    palette.text);
  RECT caption{bounds.left + px(12), bounds.bottom - px(29),
               bounds.right - px(34), bounds.bottom - px(9)};
  settings_ui::Text(item.hDC, small_font_, ThemeCaption(theme), caption,
                    palette.muted);
  RECT radio{bounds.right - px(28), bounds.bottom - px(39),
             bounds.right - px(13), bounds.bottom - px(24)};
  settings_ui::RoundedBox(item.hwndItem, item.hDC, radio, palette.panel,
                          selected ? palette.accent : palette.border, px(7));
  if (selected) {
    RECT dot{radio.left + px(4), radio.top + px(4), radio.right - px(4),
             radio.bottom - px(4)};
    settings_ui::RoundedBox(item.hwndItem, item.hDC, dot, palette.accent,
                            palette.accent, px(4));
  }

  if ((item.itemState & ODS_FOCUS) && !(item.itemState & ODS_NOFOCUSRECT)) {
    RECT focus = bounds;
    InflateRect(&focus, -px(4), -px(4));
    DrawFocusRect(item.hDC, &focus);
  }
}
}  // namespace mouse_cursor_detector
