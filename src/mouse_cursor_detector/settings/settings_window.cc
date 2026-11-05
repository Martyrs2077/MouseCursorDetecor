#include "mouse_cursor_detector/settings/settings_window.h"

#include <commctrl.h>
#include <commdlg.h>
#include <objidl.h>

#include <gdiplus.h>

#include <algorithm>
#include <chrono>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "mouse_cursor_detector/core/startup_manager.h"
#include "mouse_cursor_detector/resources/resource.h"
#include "mouse_cursor_detector/settings/settings_control_frames.h"
#include "mouse_cursor_detector/settings/settings_control_ids.h"
#include "mouse_cursor_detector/settings/settings_ui.h"

namespace mouse_cursor_detector {
namespace {
namespace ids = settings_controls;
constexpr int kSourcePath = IDC_MATERIAL_PATH;
constexpr int kSelectedFile = IDC_MATERIAL_IMPORT_HINT;
constexpr UINT_PTR kImportTimer = 9;
constexpr int kBuiltinModeCount = 2;

LRESULT DrawSlider(const NMCUSTOMDRAW& draw) {
  if (draw.dwDrawStage != CDDS_PREPAINT) return CDRF_DODEFAULT;
  const HWND slider = draw.hdr.hwndFrom;
  RECT client{}, channel{}, thumb{};
  GetClientRect(slider, &client);
  SendMessageW(slider, TBM_GETCHANNELRECT, 0,
               reinterpret_cast<LPARAM>(&channel));
  SendMessageW(slider, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
  SetDCBrushColor(draw.hdc, settings_ui::kPanel);
  FillRect(draw.hdc, &client, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

  // Use the native thumb geometry so drawing and hit testing stay aligned.
  {
    Gdiplus::Graphics graphics(draw.hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    const float scale = GetDpiForWindow(slider) / 96.0f;
    const int left = static_cast<int>(channel.left);
    const int right = static_cast<int>(channel.right);
    const int x = (thumb.left + thumb.right) / 2;
    const int y = (thumb.top + thumb.bottom) / 2;
    Gdiplus::Pen track(settings_ui::Color(settings_ui::kBorder), 3.0f * scale);
    Gdiplus::Pen active(settings_ui::Color(settings_ui::kAccent), 3.0f * scale);
    graphics.DrawLine(&track, left, y, right, y);
    graphics.DrawLine(&active, left, y, std::clamp(x, left, right), y);
    Gdiplus::SolidBrush brush(settings_ui::Color(settings_ui::kPanel));
    Gdiplus::Pen outline(settings_ui::Color(settings_ui::kAccent),
                         settings_ui::BorderWidth(slider));
    const int size = MulDiv(12, GetDpiForWindow(slider), 96);
    const float stroke = settings_ui::BorderWidth(slider);
    const Gdiplus::RectF ellipse(x - size / 2.0f + stroke / 2.0f,
                                 y - size / 2.0f + stroke / 2.0f, size - stroke,
                                 size - stroke);
    graphics.FillEllipse(&brush, ellipse);
    graphics.DrawEllipse(&outline, ellipse);
  }
  if (GetFocus() == slider) {
    const auto state = SendMessageW(slider, WM_QUERYUISTATE, 0, 0);
    if (!(state & UISF_HIDEFOCUS)) DrawFocusRect(draw.hdc, &client);
  }
  return CDRF_SKIPDEFAULT;
}

void DrawNavGlyph(HDC dc, HWND window, int id, const RECT& bounds,
                  COLORREF color) {
  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  Gdiplus::Pen pen(settings_ui::Color(color), settings_ui::BorderWidth(window));
  const float left = static_cast<float>(bounds.left);
  const float top = static_cast<float>(bounds.top);
  const float width = static_cast<float>(bounds.right - bounds.left);
  const float height = static_cast<float>(bounds.bottom - bounds.top);
  if (id == settings_controls::kMainTab) {
    for (int i = 0; i < 3; ++i) {
      const float y = top + height * (0.25f + i * 0.25f);
      const float knob = left + width * (i == 0 ? 0.62f : i == 1 ? 0.38f : 0.72f);
      graphics.DrawLine(&pen, left, y, left + width, y);
      graphics.DrawEllipse(&pen, knob - width * 0.10f, y - width * 0.10f,
                           width * 0.20f, width * 0.20f);
    }
  } else if (id == settings_controls::kMaterialsTab) {
    graphics.DrawRectangle(&pen, left + width * 0.12f, top + height * 0.22f,
                           width * 0.76f, height * 0.58f);
    graphics.DrawEllipse(&pen, left + width * 0.28f, top + height * 0.34f,
                         width * 0.12f, width * 0.12f);
    Gdiplus::PointF points[] = {
        {left + width * 0.18f, top + height * 0.70f},
        {left + width * 0.43f, top + height * 0.48f},
        {left + width * 0.60f, top + height * 0.60f},
        {left + width * 0.82f, top + height * 0.42f}};
    graphics.DrawLines(&pen, points, 4);
  } else {
    graphics.DrawEllipse(&pen, left + width * 0.14f, top + height * 0.14f,
                         width * 0.72f, height * 0.72f);
    Gdiplus::SolidBrush dot(settings_ui::Color(color));
    for (int i = 0; i < 4; ++i) {
      const float x = left + width * (i == 0 ? 0.34f : i == 1 ? 0.50f
                                             : i == 2 ? 0.66f
                                                      : 0.68f);
      const float y = top + height * (i == 0 ? 0.40f : i == 1 ? 0.30f
                                            : i == 2 ? 0.40f
                                                     : 0.58f);
      graphics.FillEllipse(&dot, x, y, width * 0.06f, width * 0.06f);
    }
  }
}

struct StyleNameDialog {
  const std::vector<UserStyle>* styles;
  std::wstring default_name;
  std::wstring name;
};

INT_PTR CALLBACK StyleNameProc(HWND window, UINT message, WPARAM wparam,
                               LPARAM lparam) {
  auto* data =
      reinterpret_cast<StyleNameDialog*>(GetWindowLongPtrW(window, DWLP_USER));
  if (message == WM_INITDIALOG) {
    data = reinterpret_cast<StyleNameDialog*>(lparam);
    SetWindowLongPtrW(window, DWLP_USER, lparam);
    SetWindowTextW(window, Tr(L"为光圈样式命名"));
    SetDlgItemTextW(window, IDOK, Tr(L"确定"));
    SetDlgItemTextW(window, IDCANCEL, Tr(L"取消导入"));
    const auto hint = Tr(L"请输入样式名称（区分大小写，不允许完全重名）。\n"
                         L"留空使用原文件名：") +
                      data->default_name;
    SetDlgItemTextW(window, IDC_STYLE_NAME_HINT, hint.c_str());
    settings_ui::PrepareTextTip(GetDlgItem(window, IDC_STYLE_NAME_HINT));
    SendDlgItemMessageW(window, IDC_STYLE_NAME, EM_SETLIMITTEXT,
                        kMaxStyleNameLength, 0);
    settings_ui::PrepareButton(GetDlgItem(window, IDOK));
    settings_ui::PrepareButton(GetDlgItem(window, IDCANCEL));
    settings_ui::PrepareField(GetDlgItem(window, IDC_STYLE_NAME),
                              settings_ui::kBackground);
    SendMessageW(window, DM_SETDEFID, IDOK, 0);
    RECT owner{}, dialog{};
    GetWindowRect(GetParent(window), &owner);
    GetWindowRect(window, &dialog);
    SetWindowPos(
        window, nullptr,
        owner.left +
            (owner.right - owner.left - dialog.right + dialog.left) / 2,
        owner.top + (owner.bottom - owner.top - dialog.bottom + dialog.top) / 2,
        0, 0, SWP_NOSIZE | SWP_NOZORDER);
    return TRUE;
  }
  if (!data) return FALSE;
  if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT ||
      message == WM_CTLCOLORBTN) {
    const HWND control = reinterpret_cast<HWND>(lparam);
    if (GetPropW(control, settings_ui::kFieldProperty))
      return reinterpret_cast<INT_PTR>(settings_ui::FieldBrush(
          control, reinterpret_cast<HDC>(wparam)));
    return reinterpret_cast<INT_PTR>(settings_ui::ControlBrush(
        reinterpret_cast<HDC>(wparam),
        message == WM_CTLCOLOREDIT ? settings_ui::kPanel
                                  : settings_ui::kBackground));
  }
  if (message == WM_ERASEBKGND) {
    RECT rect{};
    GetClientRect(window, &rect);
    SetDCBrushColor(reinterpret_cast<HDC>(wparam), settings_ui::kBackground);
    FillRect(reinterpret_cast<HDC>(wparam), &rect,
             static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    return TRUE;
  }
  if (message == WM_DRAWITEM) {
    const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
    if (item && item->CtlType == ODT_BUTTON) {
      settings_ui::Button(*item, item->CtlID == IDOK, settings_ui::kBackground);
      return TRUE;
    }
  }
  if (message == WM_CLOSE ||
      (message == WM_COMMAND && LOWORD(wparam) == IDCANCEL)) {
    EndDialog(window, IDCANCEL);
    return TRUE;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == IDC_STYLE_NAME &&
      HIWORD(wparam) == EN_CHANGE) {
    const HWND edit = GetDlgItem(window, IDC_STYLE_NAME);
    wchar_t buffer[kMaxStyleNameLength + 1]{};
    GetWindowTextW(edit, buffer, static_cast<int>(kMaxStyleNameLength + 1));
    auto name = TrimStyleName(buffer);
    if (name.empty()) name = TrimStyleName(data->default_name);
    settings_ui::SetFieldError(
        edit, !IsValidStyleName(name) || HasStyleName(*data->styles, name));
    return TRUE;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == IDOK) {
    wchar_t buffer[kMaxStyleNameLength + 1]{};
    GetDlgItemTextW(window, IDC_STYLE_NAME, buffer,
                    static_cast<int>(kMaxStyleNameLength + 1));
    auto name = TrimStyleName(buffer);
    if (name.empty()) name = TrimStyleName(data->default_name);
    if (!IsValidStyleName(name) || HasStyleName(*data->styles, name)) {
      settings_ui::SetFieldError(GetDlgItem(window, IDC_STYLE_NAME), true);
      MessageBoxW(window,
                  IsValidStyleName(name)
                      ? Tr(L"该样式名称已存在，请输入其他名称后再确定。")
                      : Tr(L"名称须为 1–255 个字符，且不能含控制字符。"),
                  Tr(L"样式名称不可用"), MB_OK | MB_ICONWARNING);
      SetFocus(GetDlgItem(window, IDC_STYLE_NAME));
      SendDlgItemMessageW(window, IDC_STYLE_NAME, EM_SETSEL, 0, -1);
      return TRUE;  // Keep the dialog open until a unique name is supplied.
    }
    data->name = std::move(name);
    EndDialog(window, IDOK);
    return TRUE;
  }
  return FALSE;
}

UINT ModifierMask(UINT key) {
  switch (key) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
      return MOD_CONTROL;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
      return MOD_ALT;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
      return MOD_SHIFT;
    case VK_LWIN:
    case VK_RWIN:
      return MOD_WIN;
    default:
      return 0;
  }
}

std::wstring KeyLabel(UINT key, UINT modifiers) {
  std::wstring label;
  if (modifiers & MOD_CONTROL) label += L"Ctrl + ";
  if (modifiers & MOD_ALT) label += L"Alt + ";
  if (modifiers & MOD_SHIFT) label += L"Shift + ";
  if (modifiers & MOD_WIN) label += L"Win + ";
  if (key == VK_OEM_COMMA) return label + L"，";
  if (key == VK_OEM_PERIOD) return label + L"。";
  const UINT scan = MapVirtualKeyW(key, MAPVK_VK_TO_VSC_EX);
  const LONG flags = static_cast<LONG>((scan & 0xff) << 16) |
                     ((scan & 0xff00) ? (1L << 24) : 0);
  wchar_t name[128]{};
  if (GetKeyNameTextW(flags, name, 128) > 0) {
    label += name;
  } else {
    label += L"VK " + std::to_wstring(key);
  }
  return label;
}

void AddControl(HWND parent, const wchar_t* type, const wchar_t* text,
                DWORD style, int id, int x, int y, int width, int height) {
  HWND control = CreateWindowExW(
      0, type, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height, parent,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessageW(control, WM_SETFONT,
               reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
               TRUE);
}

bool ReadNumber(HWND window, int id, int minimum, int maximum, int* value) {
  BOOL valid = FALSE;
  const UINT number = GetDlgItemInt(window, id, &valid, FALSE);
  if (!valid || number < static_cast<UINT>(minimum) ||
      number > static_cast<UINT>(maximum)) {
    return false;
  }
  *value = static_cast<int>(number);
  return true;
}
}  // namespace

SettingsWindow::SettingsWindow(Preview preview, Commit commit, Startup startup)
    : preview_(std::move(preview)),
      commit_(std::move(commit)),
      startup_(std::move(startup)) {}

SettingsWindow::~SettingsWindow() {
  if (import_future_.valid()) {
    import_future_.wait();
    import_future_ = {};
  }
  Close();
}

void SettingsWindow::Show(HWND owner, const Settings& settings) {
  if (window_) {
    ShowWindow(window_, IsIconic(window_) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(window_);
    return;
  }
  owner_ = owner;
  original_ = settings;
  draft_ = settings;
  SetUiLanguage(draft_.ui_language);
  page_ = Page::kMain;
  theme_applied_ = false;
  settings_ui::UseTheme(draft_.ui_theme);
  preview_settings_ = settings;
  scroll_dip_ = 0;
  horizontal_scroll_dip_ = 0;
  WNDCLASSW window_class{};
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = GetModuleHandleW(nullptr);
  window_class.hIcon =
      LoadIconW(window_class.hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
  window_class.hbrBackground = nullptr;
  window_class.lpszClassName = L"MouseCursorDetector.Settings.v1";
  RegisterClassW(&window_class);
  const UINT dpi = GetDpiForWindow(owner);
  RECT rect{0, 0, MulDiv(900, dpi, 96),
            MulDiv(settings_ui::kMinClientHeight, dpi, 96)};
  const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
                      WS_CLIPCHILDREN | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
  constexpr DWORD ex_style =
      WS_EX_CONTROLPARENT | WS_EX_COMPOSITED | WS_EX_APPWINDOW;
  AdjustWindowRectExForDpi(&rect, style, FALSE, ex_style, dpi);
  POINT cursor{};
  GetCursorPos(&cursor);
  MONITORINFO monitor{};
  monitor.cbSize = sizeof(monitor);
  GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &monitor);
  const int width =
      std::min(static_cast<int>(rect.right - rect.left),
               static_cast<int>(monitor.rcWork.right - monitor.rcWork.left));
  const int height =
      std::min(static_cast<int>(rect.bottom - rect.top),
               static_cast<int>(monitor.rcWork.bottom - monitor.rcWork.top));
  window_ = CreateWindowExW(
      ex_style, window_class.lpszClassName, Tr(L"鼠标提示设置"), style,
      monitor.rcWork.left +
          (monitor.rcWork.right - monitor.rcWork.left - width) / 2,
      monitor.rcWork.top +
          (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2,
      width, height, owner, nullptr, window_class.hInstance, this);
  if (!window_) {
    MessageBoxW(owner, Tr(L"无法创建设置窗口。"), Tr(L"鼠标提示"),
                MB_ICONERROR);
    return;
  }
  ShowWindow(window_, SW_SHOW);
  SetForegroundWindow(window_);
}

void SettingsWindow::CreateControls() {
  theme_applied_ = false;
  const UINT dpi = GetDpiForWindow(window_);
  for (HFONT* font : {&font_, &heading_font_, &title_font_, &small_font_}) {
    if (*font) DeleteObject(*font);
    *font = nullptr;
  }
  const auto make_font = [dpi](int size, int weight) {
    return CreateFontW(-MulDiv(size, dpi, 72), 0, 0, 0, weight, FALSE, FALSE,
                       FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                       L"Microsoft YaHei UI");
  };
  font_ = make_font(10, FW_NORMAL);
  heading_font_ = make_font(11, FW_MEDIUM);
  title_font_ = make_font(18, FW_MEDIUM);
  small_font_ = make_font(9, FW_NORMAL);
  if (number_font_) DeleteObject(number_font_);
  number_font_ =
      CreateFontW(-MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                  CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
  INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
  InitCommonControlsEx(&controls);
  const auto add = [this](const wchar_t* type, const wchar_t* text, DWORD style,
                          int id, HFONT font = nullptr) {
    AddControl(window_, type, text, style, id, 0, 0, 1, 1);
    if ((style & BS_TYPEMASK) == BS_OWNERDRAW &&
        std::wcscmp(type, L"BUTTON") == 0)
      settings_ui::PrepareButton(GetDlgItem(window_, id));
    if (std::wcscmp(type, L"EDIT") == 0)
      settings_ui::PrepareField(GetDlgItem(window_, id));
    if (std::wcscmp(type, L"COMBOBOX") == 0)
      settings_ui::PrepareCombo(GetDlgItem(window_, id));
    SendDlgItemMessageW(window_, id, WM_SETFONT,
                        reinterpret_cast<WPARAM>(font ? font : font_), TRUE);
  };
  add(L"STATIC", Tr(L"提示设置"), 0, ids::kTitle, title_font_);
  add(L"BUTTON", Tr(L"提示设置"), BS_OWNERDRAW | WS_TABSTOP, ids::kMainTab);
  add(L"BUTTON", Tr(L"素材库"), BS_OWNERDRAW | WS_TABSTOP, ids::kMaterialsTab);
  add(L"BUTTON", Tr(L"外观主题"), BS_OWNERDRAW | WS_TABSTOP, ids::kThemesTab);
  add(L"STATIC", L"", 0, ids::kThemeState, small_font_);
  const wchar_t* themes[] = {Tr(L"极简聚焦"), Tr(L"清晰原生"),
                             Tr(L"暗色工作台"), Tr(L"纸感仪表")};
  const int theme_order[] = {3, 0, 1, 2};
  for (int i = 0; i < 4; ++i)
    add(L"BUTTON", themes[i], BS_OWNERDRAW | WS_TABSTOP,
        ids::kThemeFirst + theme_order[i]);
  add(L"STATIC", Tr(L"运行中"), SS_CENTER, ids::kStatus, small_font_);
  add(L"STATIC", Tr(L"提示外观"), 0, 230, heading_font_);
  add(L"BUTTON", Tr(L"光圈提示"), BS_OWNERDRAW | WS_TABSTOP, ids::kRingChoice);
  add(L"BUTTON", Tr(L"高亮光标"), BS_OWNERDRAW | WS_TABSTOP,
      ids::kHighlightChoice);
  add(L"BUTTON", Tr(L"导入图片 / GIF"), BS_OWNERDRAW | WS_TABSTOP,
      ids::kMaterials);
  add(L"STATIC", Tr(L"当前样式"), 0, 231, small_font_);
  add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, ids::kMode);
  for (const auto* label : {Tr(L"高亮光标"), Tr(L"光圈提示")}) {
    SendDlgItemMessageW(window_, ids::kMode, CB_ADDSTRING, 0,
                        reinterpret_cast<LPARAM>(label));
  }
  add(L"STATIC", Tr(L"颜色"), 0, 233);
  add(L"BUTTON", Tr(L"选择颜色"), BS_OWNERDRAW | WS_TABSTOP, ids::kColor);
  const auto number = [&](const wchar_t* label, int label_id, int edit_id,
                          const wchar_t* unit, int unit_id, int slider_id) {
    add(L"STATIC", label, 0, label_id);
    add(TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_NOTICKS, slider_id);
    add(L"EDIT", L"", WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, edit_id);
    add(L"STATIC", unit, 0, unit_id, small_font_);
    SendDlgItemMessageW(window_, edit_id, EM_SETLIMITTEXT, 4, 0);
  };
  number(Tr(L"半径"), ids::kRadiusLabel, ids::kRadius, L"DIP", 232,
         ids::kRadiusSlider);
  number(Tr(L"线宽"), ids::kThicknessLabel, ids::kThickness, L"DIP",
         ids::kThicknessUnit, ids::kThicknessSlider);
  number(Tr(L"不透明度"), ids::kOpacityLabel, ids::kOpacity, L"%", 235,
         ids::kOpacitySlider);
  add(L"STATIC", L"", 0, ids::kRangeHint, small_font_);
  add(L"STATIC", Tr(L"实时更新"), SS_RIGHT, 253, small_font_);
  add(L"STATIC", L"", SS_OWNERDRAW, ids::kPreview);
  add(L"STATIC", L"",
      SS_LEFTNOWORDWRAP | SS_ENDELLIPSIS | SS_NOPREFIX | SS_NOTIFY,
      ids::kPreviewHint, small_font_);
  add(L"STATIC", Tr(L"触发方式"), 0, 239, heading_font_);
  add(L"EDIT", L"", WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL, ids::kHotkey);
  add(L"BUTTON", Tr(L"更改"), WS_TABSTOP | BS_OWNERDRAW, ids::kChangeHotkey);
  if (!SetWindowSubclass(GetDlgItem(window_, ids::kHotkey), HotkeyProc, 1,
                         reinterpret_cast<DWORD_PTR>(this))) {
    EnableWindow(GetDlgItem(window_, ids::kHotkey), FALSE);
    EnableWindow(GetDlgItem(window_, ids::kChangeHotkey), FALSE);
    MessageBoxW(window_, Tr(L"快捷键输入框初始化失败，请关闭设置后重试。"),
                Tr(L"鼠标提示"), MB_ICONWARNING);
  }
  add(L"STATIC", L"", 0, 240, small_font_);
  add(L"STATIC", Tr(L"保持时长"), 0, 236);
  add(L"EDIT", L"", WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, ids::kHoldSeconds);
  SendDlgItemMessageW(window_, ids::kHoldSeconds, EM_SETLIMITTEXT, 4, 0);
  add(L"STATIC", Tr(L"秒"), 0, 237, small_font_);
  add(L"STATIC", Tr(L"再次触发会重新计时；长按只触发一次"), 0, 254,
      small_font_);
  add(L"STATIC", Tr(L"开机自启动"), 0, 242);
  add(L"STATIC", Tr(L"登录 Windows 后自动运行"), 0, ids::kStartupHint,
      small_font_);
  add(L"BUTTON", Tr(L"开机自启动"), BS_OWNERDRAW | WS_TABSTOP, ids::kStartup);
  add(L"STATIC", L"", 0, ids::kHint, small_font_);
  add(L"BUTTON", Tr(L"恢复默认"), BS_OWNERDRAW | WS_TABSTOP, ids::kReset);
  add(L"BUTTON", Tr(L"取消"), BS_OWNERDRAW | WS_TABSTOP, IDCANCEL);
  add(L"BUTTON", Tr(L"保存设置"), BS_OWNERDRAW | WS_TABSTOP, IDOK);
  add(L"BUTTON", L"Language/语言", BS_OWNERDRAW | WS_TABSTOP, ids::kLanguage);
  for (const int id : {ids::kPreviewHint, ids::kHotkey, ids::kMode})
    settings_ui::PrepareTextTip(GetDlgItem(window_, id));
  LoadControls();
}

void SettingsWindow::ScrollControls(int position, bool horizontal) {
  const RECT client = settings_ui::ClientDip(window_);
  int& current = horizontal ? horizontal_scroll_dip_ : scroll_dip_;
  const int maximum =
      horizontal
          ? settings_ui::kMinClientWidth - static_cast<int>(client.right)
          : settings_ui::kMinClientHeight - static_cast<int>(client.bottom);
  const int next = std::clamp(position, 0, std::max(0, maximum));
  if (next == current) return;
  current = next;
  LayoutControls();
}

void SettingsWindow::DrawSettingsButton(const DRAWITEMSTRUCT& item) {
  if (item.CtlID >= ids::kThemeFirst && item.CtlID <= ids::kThemeLast) {
    DrawThemeCard(item);
    return;
  }
  const auto px = [this](int value) {
    return settings_ui::Scale(window_, value);
  };
  const bool disabled = (item.itemState & ODS_DISABLED) != 0;
  const bool hot =
      GetPropW(item.hwndItem, settings_ui::kButtonHotProperty) != nullptr;
  const bool focused =
      (item.itemState & ODS_FOCUS) && !(item.itemState & ODS_NOFOCUSRECT);
  if (item.CtlID >= ids::kMainTab && item.CtlID <= ids::kThemesTab) {
    const UINT selected =
        page_ == Page::kMain ? ids::kMainTab
        : page_ == Page::kMaterials ? ids::kMaterialsTab
                                    : ids::kThemesTab;
    const bool active = item.CtlID == selected;
    settings_ui::Fill(item.hDC, item.rcItem, settings_ui::kRail);
    RECT face = item.rcItem;
    if (active || hot) {
      settings_ui::RoundedBox(item.hwndItem, item.hDC, face,
                              active ? settings_ui::kPanel
                                     : settings_ui::kPanelAlt,
                              settings_ui::kBorder, px(10));
    }
    if (active) {
      RECT bar{face.left + px(1), face.top + px(10), face.left + px(4),
               face.bottom - px(10)};
      settings_ui::Fill(item.hDC, bar, settings_ui::kAccent);
    }
    RECT icon{face.left + px(13), face.top + px(12), face.left + px(30),
              face.top + px(29)};
    DrawNavGlyph(item.hDC, item.hwndItem, item.CtlID, icon,
                 active ? settings_ui::kAccent : settings_ui::kMuted);
    wchar_t label[32]{};
    GetWindowTextW(item.hwndItem, label, 32);
    RECT text{face.left + px(38), face.top, face.right - px(8), face.bottom};
    settings_ui::Text(item.hDC, active ? heading_font_ : font_, label, text,
                      active ? settings_ui::kText : settings_ui::kMuted,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (focused) {
      RECT focus = face;
      InflateRect(&focus, -px(4), -px(4));
      DrawFocusRect(item.hDC, &focus);
    }
    return;
  }
  if (item.CtlID == ids::kRingChoice || item.CtlID == ids::kHighlightChoice) {
    const bool ring = item.CtlID == ids::kRingChoice;
    const bool selected =
        draft_.mode == (ring ? IndicatorMode::kRing : IndicatorMode::kHighlight);
    RECT face = item.rcItem;
    InflateRect(&face, -px(1), -px(1));
    settings_ui::Fill(item.hDC, item.rcItem, settings_ui::kPanel);
    settings_ui::RoundedBox(
        item.hwndItem, item.hDC, face,
        selected ? settings_ui::kAccentTint : settings_ui::kPanelAlt,
        selected ? settings_ui::kAccent : settings_ui::kBorder, px(8));
    wchar_t label[32]{};
    GetWindowTextW(item.hwndItem, label, 32);
    settings_ui::Text(item.hDC, font_, label, face,
                      disabled ? settings_ui::kMuted
                      : selected ? settings_ui::kAccent
                                 : settings_ui::kMuted,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (focused) {
      RECT focus = face;
      InflateRect(&focus, -px(4), -px(4));
      DrawFocusRect(item.hDC, &focus);
    }
    return;
  }
  if (item.CtlID == ids::kColor) {
    settings_ui::Fill(item.hDC, item.rcItem, settings_ui::kPanel);
    RECT face = item.rcItem;
    InflateRect(&face, -px(1), -px(1));
    settings_ui::RoundedBox(item.hwndItem, item.hDC, face,
                            settings_ui::kPanelAlt, settings_ui::kBorder, px(8));
    RECT swatch{face.left + px(8), face.top + px(6), face.left + px(24),
                face.top + px(22)};
    settings_ui::RoundedBox(item.hwndItem, item.hDC, swatch,
                            draft_.active_style().color,
                            settings_ui::kBorder, px(4));
    const auto color = draft_.active_style().color;
    wchar_t hex[16]{};
    std::swprintf(hex, 16, L"#%02X%02X%02X",
                  static_cast<unsigned int>(GetRValue(color)),
                  static_cast<unsigned int>(GetGValue(color)),
                  static_cast<unsigned int>(GetBValue(color)));
    RECT value{face.left + px(32), face.top, face.right - px(62), face.bottom};
    settings_ui::Text(item.hDC, number_font_, hex, value,
                      disabled ? settings_ui::kMuted : settings_ui::kText,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT change{face.right - px(58), face.top, face.right - px(10),
                face.bottom};
    settings_ui::Text(item.hDC, small_font_, Tr(L"更改"), change,
                      disabled ? settings_ui::kMuted : settings_ui::kAccent,
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    if (focused) {
      RECT focus = face;
      InflateRect(&focus, -px(4), -px(4));
      DrawFocusRect(item.hDC, &focus);
    }
    return;
  }
  if (item.CtlID == ids::kStartup) {
    settings_ui::Fill(item.hDC, item.rcItem, settings_ui::kPanel);
    RECT track = item.rcItem;
    settings_ui::RoundedBox(
        item.hwndItem, item.hDC, track,
        startup_enabled_ ? settings_ui::kAccent : settings_ui::kPanelAlt,
        startup_enabled_ ? settings_ui::kAccent : settings_ui::kBorder, px(10));
    Gdiplus::Graphics graphics(item.hDC);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush knob(settings_ui::Color(
        startup_enabled_ ? settings_ui::kOnAccent : settings_ui::kMuted));
    const int knob_size = px(15);
    const int knob_x = startup_enabled_ ? track.right - knob_size - px(3)
                                        : track.left + px(3);
    graphics.FillEllipse(&knob, static_cast<float>(knob_x),
                         static_cast<float>(track.top + px(3)),
                         static_cast<float>(knob_size),
                         static_cast<float>(knob_size));
    if (focused) {
      RECT focus = track;
      InflateRect(&focus, -px(3), -px(3));
      DrawFocusRect(item.hDC, &focus);
    }
    return;
  }
  if (item.CtlID == ids::kReset) {
    settings_ui::Fill(item.hDC, item.rcItem, settings_ui::kBackground);
    wchar_t label[32]{};
    GetWindowTextW(item.hwndItem, label, 32);
    settings_ui::Text(item.hDC, font_, label, item.rcItem,
                      hot ? settings_ui::kText : settings_ui::kMuted,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (focused) {
      RECT focus = item.rcItem;
      InflateRect(&focus, -px(4), -px(4));
      DrawFocusRect(item.hDC, &focus);
    }
    return;
  }
  const bool in_panel = item.CtlID == ids::kChangeHotkey ||
                        item.CtlID == ids::kMaterials;
  settings_ui::Button(item, item.CtlID == IDOK || item.CtlID == ids::kLanguage,
                      in_panel ? settings_ui::kPanel : settings_ui::kBackground,
                      item.CtlID == ids::kChangeHotkey);
}

void SettingsWindow::RefreshPreview() {
  if (!window_) return;
  InvalidateRect(GetDlgItem(window_, ids::kPreview), nullptr, FALSE);
}

void SettingsWindow::LoadControls() {
  const settings_ui::ScopedRedraw redraw(window_);
  loading_ = true;
  settings_ui::UseTheme(draft_.ui_theme);
  const auto& style = draft_.active_style();
  const bool ring = draft_.mode == IndicatorMode::kRing;
  // Only entries already saved from the import dialog are selectable.
  while (SendDlgItemMessageW(window_, ids::kMode, CB_GETCOUNT, 0, 0) >
         kBuiltinModeCount) {
    SendDlgItemMessageW(window_, ids::kMode, CB_DELETESTRING, kBuiltinModeCount,
                        0);
  }
  int selection = draft_.is_custom() ? -1 : static_cast<int>(draft_.mode);
  for (size_t i = 0; i < original_.user_styles.size(); ++i) {
    const auto& entry = original_.user_styles[i];
    SendDlgItemMessageW(window_, ids::kMode, CB_ADDSTRING, 0,
                        reinterpret_cast<LPARAM>(entry.name.c_str()));
    if (draft_.is_custom() && entry.relative_path == draft_.style_file &&
        draft_.image_asset) {
      selection = kBuiltinModeCount + static_cast<int>(i);
    }
  }
  SendDlgItemMessageW(window_, ids::kMode, CB_SETCURSEL, selection, 0);
  SetDlgItemInt(window_, ids::kRadius, style.radius_dip, FALSE);
  SetDlgItemInt(window_, ids::kThickness, style.thickness_dip, FALSE);
  SetDlgItemInt(window_, ids::kOpacity, style.opacity_percent, FALSE);
  SetDlgItemInt(window_, ids::kHoldSeconds, draft_.hold_seconds, FALSE);
  SetDlgItemTextW(window_, ids::kRangeHint,
                  draft_.is_custom() ? Tr(L"半尺寸：8–256 DIP；时长：1–3600 秒")
                                     : Tr(L"半径：8–100 DIP；时长：1–3600 秒"));
  EnableWindow(GetDlgItem(window_, ids::kColor), !draft_.is_custom());
  SetDlgItemTextW(window_, ids::kRadiusLabel,
                  draft_.is_custom() ? Tr(L"半尺寸") : Tr(L"半径"));
  for (const int id : {ids::kMode, ids::kRingChoice, ids::kHighlightChoice,
                       ids::kReset, IDOK}) {
    EnableWindow(GetDlgItem(window_, id), !import_future_.valid());
  }
  EnableWindow(GetDlgItem(window_, ids::kMaterials), !import_future_.valid());
  SyncSliders();
  LoadMaterialControls();
  UpdateHotkeyLabel();
  SetDlgItemTextW(window_, ids::kHint,
                  Tr(L"保留系统光标 · 设置保存在 config/settings.json"));
  std::wstring preview_label = ring ? Tr(L"光圈提示") : Tr(L"高亮光标");
  if (draft_.is_custom()) {
    preview_label = Tr(L"自定义素材");
    for (const auto& entry : draft_.user_styles) {
      if (entry.relative_path == draft_.style_file) {
        preview_label = entry.name;
        break;
      }
    }
  }
  preview_label += Tr(L"  ·  等比预览");
  SetDlgItemTextW(window_, ids::kPreviewHint, preview_label.c_str());
  const auto color = style.color;
  wchar_t color_label[32]{};
  std::swprintf(color_label, 32, L"     #%02X%02X%02X",
                static_cast<unsigned int>(GetRValue(color)),
                static_cast<unsigned int>(GetGValue(color)),
                static_cast<unsigned int>(GetBValue(color)));
  SetDlgItemTextW(window_, ids::kColor, color_label);
  RefreshStartup();
  ApplyTheme();
  LayoutControls();
  RefreshPreview();
  for (const int id : {ids::kRingChoice, ids::kHighlightChoice})
    InvalidateRect(GetDlgItem(window_, id), nullptr, FALSE);
  if (draft_.mode == IndicatorMode::kGif && draft_.image_asset)
    SetTimer(window_, settings_ui::kAnimationTimer, 33, nullptr);
  else
    KillTimer(window_, settings_ui::kAnimationTimer);
  loading_ = false;
  RefreshLanguage();
}

void SettingsWindow::ToggleLanguage() {
  if (naming_style_) return;
  draft_.ui_language = draft_.ui_language == UiLanguage::kChinese
                           ? UiLanguage::kEnglish
                           : UiLanguage::kChinese;
  // Language is an independent draft: do not validate or replace edit values.
  preview_settings_.ui_language = draft_.ui_language;
  materials_before_.ui_language = draft_.ui_language;
  materials_previous_preview_.ui_language = draft_.ui_language;
  preview_(preview_settings_);
  RefreshLanguage();
}

void SettingsWindow::RefreshLanguage() {
  SetUiLanguage(draft_.ui_language);
  const settings_ui::ScopedRedraw redraw(window_);
  const bool was_loading = loading_;
  loading_ = true;
  SetWindowTextW(window_, Tr(L"鼠标提示设置"));
  struct Label {
    int id;
    const wchar_t* text;
  };
  const Label labels[] = {
      {ids::kMainTab, L"提示设置"},
      {ids::kMaterialsTab, L"素材库"},
      {ids::kThemesTab, L"外观主题"},
      {230, L"提示外观"},
      {ids::kRingChoice, L"光圈提示"},
      {ids::kHighlightChoice, L"高亮光标"},
      {ids::kMaterials, L"导入图片 / GIF"},
      {231, L"当前样式"},
      {233, L"颜色"},
      {ids::kThicknessLabel, L"线宽"},
      {ids::kOpacityLabel, L"不透明度"},
      {253, L"实时更新"},
      {239, L"触发方式"},
      {ids::kChangeHotkey, L"更改"},
      {236, L"保持时长"},
      {237, L"秒"},
      {254, L"再次触发会重新计时；长按只触发一次"},
      {242, L"开机自启动"},
      {ids::kStartup, L"开机自启动"},
      {ids::kReset, L"恢复默认"},
      {IDCANCEL, L"取消"},
      {IDOK, L"保存设置"},
      {ids::kHint, L"保留系统光标 · 设置保存在 config/settings.json"},
  };
  for (const auto& label : labels)
    settings_ui::SetControlText(window_, label.id, Tr(label.text));
  settings_ui::SetControlText(window_, ids::kRadiusLabel,
                              Tr(draft_.is_custom() ? L"半尺寸" : L"半径"));
  settings_ui::SetControlText(
      window_, ids::kRangeHint,
      Tr(draft_.is_custom() ? L"半尺寸：8–256 DIP；时长：1–3600 秒"
                            : L"半径：8–100 DIP；时长：1–3600 秒"));
  // Replace only built-in labels. User material names and selection are data.
  const LRESULT selection =
      SendDlgItemMessageW(window_, ids::kMode, CB_GETCURSEL, 0, 0);
  for (int index = 0; index < kBuiltinModeCount; ++index) {
    SendDlgItemMessageW(window_, ids::kMode, CB_DELETESTRING, index, 0);
    SendDlgItemMessageW(
        window_, ids::kMode, CB_INSERTSTRING, index,
        reinterpret_cast<LPARAM>(Tr(index == 0 ? L"高亮光标" : L"光圈提示")));
  }
  SendDlgItemMessageW(window_, ids::kMode, CB_SETCURSEL, selection, 0);
  std::wstring preview_label =
      Tr(draft_.mode == IndicatorMode::kRing ? L"光圈提示" : L"高亮光标");
  if (draft_.is_custom()) {
    preview_label = Tr(L"自定义素材");
    for (const auto& entry : draft_.user_styles) {
      if (entry.relative_path == draft_.style_file) {
        preview_label = entry.name;
        break;
      }
    }
  }
  preview_label += Tr(L"  ·  等比预览");
  settings_ui::SetControlText(window_, ids::kPreviewHint,
                              preview_label.c_str());
  if (materials_window_) {
    const Label material_labels[] = {
        {IDC_MATERIAL_TITLE, L"素材库"},
        {IDC_MATERIAL_PATH_LABEL, L"导入新素材"},
        {IDC_MATERIAL_BROWSE, L"浏览"},
        {IDC_MATERIAL_IMPORT, L"导入素材"},
        {IDC_MATERIAL_PREVIEW_LABEL, L"素材预览"},
        {IDC_MATERIAL_FORMATS, L"最大 20 MiB · 导入后可命名并预览"},
        {IDOK, L"保存样式"},
        {IDCANCEL, L"取消"},
    };
    SetWindowTextW(materials_window_, Tr(L"素材库"));
    for (const auto& label : material_labels)
      settings_ui::SetControlText(materials_window_, label.id, Tr(label.text));
    SendDlgItemMessageW(
        materials_window_, IDC_MATERIAL_PATH, EM_SETCUEBANNER, TRUE,
        reinterpret_cast<LPARAM>(Tr(L"选择文件，或粘贴完整路径")));
    LoadMaterialControls();
  }
  RefreshStartup();
  ApplyTheme();
  LayoutControls();
  loading_ = was_loading;
  RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void SettingsWindow::RefreshStartup() {
  if (!window_) return;
  const auto state = QueryStartup();
  startup_enabled_ = state.registered;
  SetDlgItemTextW(window_, ids::kStartupHint,
                  state.registered && !state.current_path
                      ? Tr(L"自启动路径已变化，请点击修正")
                      : Tr(L"登录 Windows 后自动运行"));
  SendDlgItemMessageW(window_, ids::kStartup, BM_SETCHECK,
                      state.registered ? BST_CHECKED : BST_UNCHECKED, 0);
  InvalidateRect(GetDlgItem(window_, ids::kStartup), nullptr, FALSE);
}

bool SettingsWindow::ReadControls(bool report_error) {
  auto& style = draft_.is_custom()
                    ? draft_.custom
                    : (draft_.mode == IndicatorMode::kRing ? draft_.ring
                                                           : draft_.highlight);
  int radius = 0;
  int thickness = style.thickness_dip;
  int opacity = 0;
  int hold_seconds = 0;
  const bool ring = draft_.mode == IndicatorMode::kRing;
  const bool radius_valid = ReadNumber(
      window_, ids::kRadius, 8, draft_.is_custom() ? 256 : 100, &radius);
  const bool thickness_valid =
      !ring || (ReadNumber(window_, ids::kThickness, 1, 20, &thickness) &&
                (!radius_valid || thickness < radius * 2));
  const bool opacity_valid = ReadNumber(
      window_, ids::kOpacity, 10,
      draft_.mode == IndicatorMode::kHighlight ? 50 : 100, &opacity);
  const bool hold_valid =
      ReadNumber(window_, ids::kHoldSeconds, 1, 3600, &hold_seconds);
  settings_ui::SetFieldError(GetDlgItem(window_, ids::kRadius), !radius_valid);
  settings_ui::SetFieldError(GetDlgItem(window_, ids::kThickness),
                             !thickness_valid);
  settings_ui::SetFieldError(GetDlgItem(window_, ids::kOpacity), !opacity_valid);
  settings_ui::SetFieldError(GetDlgItem(window_, ids::kHoldSeconds), !hold_valid);
  const bool valid =
      radius_valid && thickness_valid && opacity_valid && hold_valid;
  if (!valid) {
    if (report_error) {
      if (page_ == Page::kThemes) SelectPage(Page::kMain);
      MessageBoxW(window_,
                  Tr(L"请输入范围内的数值；保持时长为 1–3600 "
                     L"秒，光圈线宽须小于半径的两倍。"),
                  Tr(L"设置未保存"), MB_OK | MB_ICONWARNING);
      int ignored = 0;
      const int focus =
          !ReadNumber(window_, ids::kRadius, 8, draft_.is_custom() ? 256 : 100,
                      &ignored)
              ? ids::kRadius
          : ring && (!ReadNumber(window_, ids::kThickness, 1, 20, &ignored) ||
                     ignored >= radius * 2)
              ? ids::kThickness
          : !ReadNumber(window_, ids::kOpacity, 10,
                        draft_.mode == IndicatorMode::kHighlight ? 50 : 100,
                        &ignored)
              ? ids::kOpacity
              : ids::kHoldSeconds;
      SetFocus(GetDlgItem(window_, focus));
      SendDlgItemMessageW(window_, focus, EM_SETSEL, 0, -1);
    }
    return false;
  }
  style.radius_dip = radius;
  style.thickness_dip = thickness;
  style.opacity_percent = opacity;
  draft_.hold_seconds = hold_seconds;
  return true;
}

void SettingsWindow::PreviewDraft() {
  RefreshPreview();
  // An empty custom mode is selectable so the user can reach the import dialog.
  // Until a valid asset is supplied, keep the last usable visual settings.
  if (draft_.is_custom() &&
      (!draft_.image_asset ||
       (draft_.mode == IndicatorMode::kGif && !draft_.image_asset->is_gif)))
    return;
  preview_(draft_);
  preview_settings_ = draft_;
}

void SettingsWindow::SyncSliders() {
  const auto& style = draft_.active_style();
  const auto sync = [this](int id, int minimum, int maximum, int value) {
    const HWND slider = GetDlgItem(window_, id);
    bool changed = false;
    if (SendMessageW(slider, TBM_GETRANGEMIN, 0, 0) != minimum) {
      SendMessageW(slider, TBM_SETRANGEMIN, FALSE, minimum);
      changed = true;
    }
    if (SendMessageW(slider, TBM_GETRANGEMAX, 0, 0) != maximum) {
      SendMessageW(slider, TBM_SETRANGEMAX, FALSE, maximum);
      changed = true;
    }
    if (SendMessageW(slider, TBM_GETPOS, 0, 0) != value) {
      SendMessageW(slider, TBM_SETPOS, FALSE, value);
      changed = true;
    }
    // Native invalidation only covers the thumb, but our fill spans the track.
    if (changed) InvalidateRect(slider, nullptr, FALSE);
  };
  sync(ids::kRadiusSlider, 8, draft_.is_custom() ? 256 : 100, style.radius_dip);
  sync(ids::kOpacitySlider, 10,
       draft_.mode == IndicatorMode::kHighlight ? 50 : 100,
       style.opacity_percent);
  sync(ids::kThicknessSlider, 1, std::min(20, style.radius_dip * 2 - 1),
       style.thickness_dip);
}

void SettingsWindow::HandleSlider(HWND slider) {
  if (loading_ || !slider) return;
  int edit = 0;
  switch (GetDlgCtrlID(slider)) {
    case ids::kRadiusSlider:
      edit = ids::kRadius;
      break;
    case ids::kOpacitySlider:
      edit = ids::kOpacity;
      break;
    case ids::kThicknessSlider:
      edit = ids::kThickness;
      break;
    default:
      return;
  }
  const int value = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
  InvalidateRect(slider, nullptr, FALSE);
  loading_ = true;
  SetDlgItemInt(window_, edit, value, FALSE);
  settings_ui::SetFieldError(GetDlgItem(window_, edit), false);
  auto& style = draft_.is_custom()
                    ? draft_.custom
                    : (draft_.mode == IndicatorMode::kRing ? draft_.ring
                                                           : draft_.highlight);
  if (edit == ids::kRadius)
    style.radius_dip = value;
  else if (edit == ids::kThickness)
    style.thickness_dip = value;
  else
    style.opacity_percent = value;
  if (edit == ids::kRadius && draft_.mode == IndicatorMode::kRing) {
    style.thickness_dip =
        std::min(style.thickness_dip, std::min(20, value * 2 - 1));
    SetDlgItemInt(window_, ids::kThickness, style.thickness_dip, FALSE);
    settings_ui::SetFieldError(GetDlgItem(window_, ids::kThickness), false);
  }
  loading_ = false;
  // A slider value is valid on its own, even while another edit is incomplete.
  // Keep that edit's error state and the last valid value in the draft.
  ReadControls(false);
  SyncSliders();
  PreviewDraft();
  // WM_PAINT can otherwise be deferred by a stream of thumb-track messages.
  UpdateWindow(GetDlgItem(window_, ids::kPreview));
}

void SettingsWindow::UpdateHotkeyLabel() {
  auto label = KeyLabel(draft_.hotkey_vk, draft_.hotkey_modifiers);
  if (draft_.hotkey_secondary_vk) {
    label += L" + " + KeyLabel(draft_.hotkey_secondary_vk, 0);
  }
  settings_ui::SetControlText(window_, ids::kHotkey, label.c_str());
}

LRESULT CALLBACK SettingsWindow::HotkeyProc(HWND window, UINT message,
                                            WPARAM wparam, LPARAM lparam,
                                            UINT_PTR subclass_id,
                                            DWORD_PTR reference) {
  auto* self = reinterpret_cast<SettingsWindow*>(reference);
  if (message == WM_SETFOCUS) self->captured_keys_.fill(false);
  if (message == WM_GETDLGCODE) return DLGC_WANTALLKEYS | DLGC_WANTCHARS;
  if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN ||
      message == WM_KEYUP || message == WM_SYSKEYUP) {
    UINT key = static_cast<UINT>(wparam);
    if (key == VK_SHIFT) {
      key = MapVirtualKeyW((lparam >> 16) & 0xff, MAPVK_VSC_TO_VK_EX);
    } else if (key == VK_CONTROL) {
      key = lparam & (1L << 24) ? VK_RCONTROL : VK_LCONTROL;
    } else if (key == VK_MENU) {
      key = lparam & (1L << 24) ? VK_RMENU : VK_LMENU;
    }
    if (key == 0 || key >= 0xff) return 0;
    const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    self->captured_keys_[key] = down;
    if (!down || (lparam & (1L << 30))) return 0;
    UINT modifiers = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;
    if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) {
      modifiers |= MOD_WIN;
    }
    UINT primary = 0;
    UINT secondary = 0;
    for (UINT candidate = 1; candidate < 0xff; ++candidate) {
      if (!self->captured_keys_[candidate] || ModifierMask(candidate)) continue;
      if (!primary) {
        primary = candidate;
      } else if (!secondary) {
        secondary = candidate;
      } else {
        return 0;  // Capture at most two ordinary keys, plus modifiers.
      }
    }
    if (!primary) primary = key;
    self->draft_.hotkey_vk = primary;
    self->draft_.hotkey_secondary_vk = secondary;
    self->draft_.hotkey_modifiers = modifiers & ~ModifierMask(primary);
    self->UpdateHotkeyLabel();
    return 0;
  }
  if (message == WM_KEYUP || message == WM_SYSKEYUP || message == WM_CHAR ||
      message == WM_SYSCHAR) {
    return 0;
  }
  if (message == WM_NCDESTROY) {
    RemoveWindowSubclass(window, HotkeyProc, subclass_id);
  }
  return DefSubclassProc(window, message, wparam, lparam);
}

void SettingsWindow::Command(int id, int notification) {
  if (loading_) return;
  if (id == ids::kLanguage) {
    ToggleLanguage();
    return;
  }
  if (id >= ids::kMainTab && id <= ids::kThemesTab) {
    SelectPage(id == ids::kMainTab        ? Page::kMain
               : id == ids::kMaterialsTab ? Page::kMaterials
                                          : Page::kThemes);
    return;
  }
  if (id >= ids::kThemeFirst && id <= ids::kThemeLast) {
    if (naming_style_ || import_future_.valid()) return;
    draft_.ui_theme = static_cast<UiTheme>(id - ids::kThemeFirst);
    ApplyTheme();
    return;
  }
  if (page_ == Page::kMaterials && (id == IDOK || id == IDCANCEL)) {
    CloseMaterials(id == IDOK);
    return;
  }
  if (naming_style_ || (import_future_.valid() &&
                        (id == ids::kMode || id == ids::kRingChoice ||
                         id == ids::kHighlightChoice || id == ids::kMaterials ||
                         id == ids::kReset || id == IDOK)))
    return;
  if (id == ids::kRingChoice || id == ids::kHighlightChoice) {
    if (!ReadControls(true)) return;
    draft_.mode = id == ids::kRingChoice ? IndicatorMode::kRing
                                         : IndicatorMode::kHighlight;
    LoadControls();
    PreviewDraft();
  } else if (id == ids::kChangeHotkey) {
    SetFocus(GetDlgItem(window_, ids::kHotkey));
  } else if (id == ids::kMode && notification == CBN_SELCHANGE) {
    const LRESULT index =
        SendDlgItemMessageW(window_, ids::kMode, CB_GETCURSEL, 0, 0);
    if (!ReadControls(true)) {
      LoadControls();
      return;
    }
    if (index >= kBuiltinModeCount &&
        index - kBuiltinModeCount <
            static_cast<LRESULT>(original_.user_styles.size())) {
      BeginSelectStyle(static_cast<size_t>(index - kBuiltinModeCount));
      return;
    }
    if (index < 0 || index >= kBuiltinModeCount) {
      LoadControls();
      return;
    }
    draft_.mode = static_cast<IndicatorMode>(index);
    LoadControls();
    PreviewDraft();
  } else if ((id == ids::kRadius || id == ids::kThickness ||
              id == ids::kOpacity || id == ids::kHoldSeconds) &&
             notification == EN_CHANGE) {
    if (ReadControls(false)) {
      SyncSliders();
      PreviewDraft();
    }
  } else if (id == ids::kColor) {
    COLORREF custom[16]{};
    auto& style =
        draft_.mode == IndicatorMode::kRing ? draft_.ring : draft_.highlight;
    CHOOSECOLORW picker{};
    picker.lStructSize = sizeof(picker);
    picker.hwndOwner = window_;
    picker.rgbResult = style.color;
    picker.lpCustColors = custom;
    picker.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorW(&picker)) {
      style.color = picker.rgbResult;
      LoadControls();
      PreviewDraft();
    }
  } else if (id == ids::kStartup) {
    startup_();
    RefreshStartup();
  } else if (id == ids::kMaterials) {
    ShowMaterials();
  } else if (id == ids::kReset) {
    auto styles = std::move(draft_.user_styles);
    draft_ = Settings{};
    draft_.user_styles = std::move(styles);
    LoadControls();
    PreviewDraft();
  } else if (id == IDOK) {
    if (import_future_.valid()) return;
    if (!ReadControls(true)) return;
    if (draft_.is_custom() &&
        (!draft_.image_asset ||
         (draft_.mode == IndicatorMode::kGif && !draft_.image_asset->is_gif))) {
      MessageBoxW(window_, Tr(L"请先在导入光圈样式中导入适用的图片或 GIF。"),
                  Tr(L"尚无可用素材"), MB_OK | MB_ICONWARNING);
      return;
    }
    if (!draft_.hotkey_vk) {
      settings_ui::SetFieldError(GetDlgItem(window_, ids::kHotkey), true);
      MessageBoxW(window_, Tr(L"请点击快捷键输入框，按下单键或组合键。"),
                  Tr(L"设置未保存"), MB_ICONWARNING);
      return;
    }
    if (commit_(draft_)) {
      original_ = draft_;
      preview_settings_ = draft_;
      ApplyTheme();
      MessageBoxW(window_, Tr(L"设置已保存。"), Tr(L"保存成功"),
                  MB_OK | MB_ICONINFORMATION);
    }
  } else if (id == IDCANCEL) {
    Close();
  }
}

LRESULT CALLBACK SettingsWindow::WindowProc(HWND window, UINT message,
                                            WPARAM wparam, LPARAM lparam) {
  auto* self = reinterpret_cast<SettingsWindow*>(
      GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    self = static_cast<SettingsWindow*>(
        reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
    self->window_ = window;
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  if (self) {
    switch (message) {
      case WM_CREATE:
        self->CreateControls();
        return 0;
      case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) self->LayoutControls();
        return 0;
      case WM_GETMINMAXINFO: {
        const UINT dpi = GetDpiForWindow(window);
        RECT rect{0, 0, MulDiv(settings_ui::kMinClientWidth, dpi, 96),
                  MulDiv(settings_ui::kMinClientHeight, dpi, 96)};
        AdjustWindowRectExForDpi(
            &rect, static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE)),
            FALSE, WS_EX_CONTROLPARENT, dpi);
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                        &monitor);
        limits->ptMinTrackSize = {
            std::min(rect.right - rect.left,
                     monitor.rcWork.right - monitor.rcWork.left),
            std::min(rect.bottom - rect.top,
                     monitor.rcWork.bottom - monitor.rcWork.top)};
        return 0;
      }
      case WM_VSCROLL:
      case WM_HSCROLL: {
        if (message == WM_HSCROLL && lparam) {
          self->HandleSlider(reinterpret_cast<HWND>(lparam));
          return 0;
        }
        const bool horizontal = message == WM_HSCROLL;
        SCROLLINFO scroll{};
        scroll.cbSize = sizeof(scroll);
        scroll.fMask = SIF_ALL;
        GetScrollInfo(window, horizontal ? SB_HORZ : SB_VERT, &scroll);
        int position = scroll.nPos;
        switch (LOWORD(wparam)) {
          case SB_LINEUP:
            position -= 32;
            break;
          case SB_LINEDOWN:
            position += 32;
            break;
          case SB_PAGEUP:
            position -= scroll.nPage;
            break;
          case SB_PAGEDOWN:
            position += scroll.nPage;
            break;
          case SB_THUMBTRACK:
            position = scroll.nTrackPos;
            break;
          case SB_TOP:
            position = 0;
            break;
          case SB_BOTTOM:
            position = scroll.nMax;
            break;
          default:
            return 0;
        }
        self->ScrollControls(position, horizontal);
        return 0;
      }
      case WM_MOUSEWHEEL:
        self->ScrollControls(self->scroll_dip_ -
                             GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA * 48);
        return 0;
      case WM_MOUSEHWHEEL:
        self->ScrollControls(
            self->horizontal_scroll_dip_ +
                GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA * 48,
            true);
        return 0;
      case DM_GETDEFID:
        return MAKELRESULT(IDOK, DC_HASDEFID);
      case WM_ERASEBKGND:
        // Paint the background and panels together in the composited pass.
        return 1;
      case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT bounds{};
        GetClientRect(window, &bounds);
        {
          const settings_ui::PaintBuffer buffer(dc, bounds);
          self->PaintWindow(buffer.dc());
        }
        EndPaint(window, &paint);
        return 0;
      }
      case WM_CTLCOLOREDIT:
      case WM_CTLCOLORLISTBOX:
      case WM_CTLCOLORBTN:
      case WM_CTLCOLORSTATIC: {
        const HWND control = reinterpret_cast<HWND>(lparam);
        if (GetPropW(control, settings_ui::kFieldProperty))
          return reinterpret_cast<LRESULT>(settings_ui::FieldBrush(
              control, reinterpret_cast<HDC>(wparam)));
        const int id = GetDlgCtrlID(control);
        const bool muted = id == ids::kHint || id == ids::kThemeState ||
                           id == ids::kStartupHint || id == ids::kPreviewHint ||
                           id == ids::kRangeHint || id == ids::kRadiusLabel ||
                           id == ids::kThicknessLabel || id == ids::kThicknessUnit ||
                           id == ids::kOpacityLabel || id == 231 || id == 232 ||
                           id == 233 || id == 235 || id == 236 || id == 237 ||
                           id == 240 || id == 242 || id == 254;
        const bool on_background =
            id == ids::kTitle || id == ids::kHint || id == ids::kStatus ||
            id == ids::kThemeState || id == ids::kPreviewHint ||
            id == ids::kRangeHint;
        const COLORREF background =
            on_background ? settings_ui::kBackground : settings_ui::kPanel;
        const COLORREF text = muted ? settings_ui::kMuted : settings_ui::kText;
        return reinterpret_cast<LRESULT>(settings_ui::ControlBrush(
            reinterpret_cast<HDC>(wparam), background, text));
      }
      case WM_COMMAND:
        self->Command(LOWORD(wparam), HIWORD(wparam));
        return 0;
      case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
        if (item && item->CtlType == ODT_BUTTON) {
          const settings_ui::PaintBuffer buffer(item->hDC, item->rcItem);
          DRAWITEMSTRUCT buffered = *item;
          buffered.hDC = buffer.dc();
          self->DrawSettingsButton(buffered);
          return TRUE;
        }
        if (item && item->CtlID == ids::kPreview) {
          // DrawMaterialPreview already buffers the stage and GIF frames.
          self->DrawMaterialPreview(*item);
          return TRUE;
        }
        break;
      }
      case WM_NOTIFY: {
        const auto* header = reinterpret_cast<const NMHDR*>(lparam);
        if (header && header->code == NM_CUSTOMDRAW &&
            (header->idFrom == ids::kRadiusSlider ||
             header->idFrom == ids::kOpacitySlider ||
             header->idFrom == ids::kThicknessSlider)) {
          NMCUSTOMDRAW draw = *reinterpret_cast<const NMCUSTOMDRAW*>(lparam);
          if (draw.dwDrawStage != CDDS_PREPAINT) return CDRF_DODEFAULT;
          RECT bounds{};
          GetClientRect(draw.hdr.hwndFrom, &bounds);
          const settings_ui::PaintBuffer buffer(draw.hdc, bounds);
          draw.hdc = buffer.dc();
          return DrawSlider(draw);
        }
        break;
      }
      case WM_CLOSE:
        // Hide the settings taskbar button and preserve this editing session.
        if (!self->naming_style_) ShowWindow(window, SW_HIDE);
        return 0;
      case WM_TIMER:
        if (wparam == settings_ui::kAnimationTimer) {
          if (self->page_ == Page::kMain && IsWindowVisible(window) &&
              !IsIconic(window) && self->draft_.mode == IndicatorMode::kGif &&
              self->draft_.image_asset)
            InvalidateRect(GetDlgItem(window, ids::kPreview), nullptr, FALSE);
          return 0;
        }
        if (wparam == kImportTimer) {
          self->FinishImport();
          return 0;
        }
        break;
      case WM_DPICHANGED: {
        self->ReadControls(false);
        self->loading_ = true;
        const auto* rect = reinterpret_cast<RECT*>(lparam);
        SetWindowPos(window, nullptr, rect->left, rect->top,
                     rect->right - rect->left, rect->bottom - rect->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        // Preserve the embedded import page and its in-progress state.
        for (HWND child = GetWindow(window, GW_CHILD); child;) {
          const HWND next = GetWindow(child, GW_HWNDNEXT);
          if (child != self->materials_window_) DestroyWindow(child);
          child = next;
        }
        self->CreateControls();
        if (self->materials_window_) {
          self->CreateMaterialFonts();
          self->LayoutMaterials();
        }
        return 0;
      }
      case WM_NCDESTROY:
        KillTimer(window, settings_ui::kAnimationTimer);
        for (HFONT* font :
             {&self->font_, &self->heading_font_, &self->title_font_,
              &self->small_font_, &self->number_font_}) {
          if (*font) DeleteObject(*font);
          *font = nullptr;
        }
        self->window_ = nullptr;
        break;
    }
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

void SettingsWindow::Close() {
  if (!window_) return;
  if (naming_style_) return;
  if (import_future_.valid()) {
    MessageBoxW(window_, Tr(L"正在导入素材，请稍候再关闭设置。"),
                Tr(L"素材导入"), MB_OK);
    return;
  }
  if (materials_window_) CloseMaterials(false);
  KillTimer(window_, kImportTimer);
  KillTimer(window_, settings_ui::kAnimationTimer);
  preview_(original_);
  settings_ui::UseTheme(original_.ui_theme);
  DestroyWindow(window_);
}

bool SettingsWindow::HandleDialogMessage(MSG* message) {
  if (!window_) return false;
  const bool handled = IsDialogMessageW(window_, message) != FALSE;
  if (handled && window_) {
    HWND focus = GetFocus();
    if (focus && IsChild(window_, focus)) {
      RECT rect{}, client{};
      GetWindowRect(focus, &rect);
      MapWindowPoints(nullptr, window_, reinterpret_cast<POINT*>(&rect), 2);
      GetClientRect(window_, &client);
      const UINT dpi = GetDpiForWindow(window_);
      const int id = GetDlgCtrlID(focus);
      const bool navigation = GetParent(focus) == window_ &&
                              id >= ids::kMainTab && id <= ids::kThemesTab;
      const int left_limit = navigation ? 0 : settings_ui::Scale(window_, 164);
      if (rect.top < 0)
        ScrollControls(scroll_dip_ + MulDiv(rect.top, 96, dpi) - 8);
      else if (rect.bottom > client.bottom)
        ScrollControls(scroll_dip_ +
                       MulDiv(rect.bottom - client.bottom, 96, dpi) + 8);
      if (rect.left < left_limit)
        ScrollControls(horizontal_scroll_dip_ +
                           MulDiv(rect.left - left_limit, 96, dpi) - 8,
                       true);
      else if (rect.right > client.right)
        ScrollControls(horizontal_scroll_dip_ +
                           MulDiv(rect.right - client.right, 96, dpi) + 8,
                       true);
    }
  }
  return handled;
}

bool SettingsWindow::IsCapturingHotkey() const {
  return window_ && GetFocus() == GetDlgItem(window_, ids::kHotkey);
}

void SettingsWindow::BrowseStyle() {
  if (import_future_.valid() || !materials_window_) return;
  std::vector<wchar_t> path(32768);
  OPENFILENAMEW dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = materials_window_;
  dialog.lpstrFilter = Tr(
      L"图片与 "
      L"GIF\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.ico;*.gif;*.jxr;*.webp;*."
      L"heic;*.avif\0所有文件\0*.*\0");
  dialog.lpstrFile = path.data();
  dialog.nMaxFile = static_cast<DWORD>(path.size());
  dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (GetOpenFileNameW(&dialog))
    SetDlgItemTextW(materials_window_, kSourcePath, path.data());
}

void SettingsWindow::BeginImport() {
  if (import_future_.valid() || !materials_window_ || !ReadControls(true))
    return;
  if (draft_.user_styles.size() >= kMaxUserStyles) {
    MessageBoxW(materials_window_, Tr(L"样式库最多保存 512 个素材。"),
                Tr(L"素材导入"), MB_OK);
    return;
  }
  const HWND edit = GetDlgItem(materials_window_, kSourcePath);
  const int length = GetWindowTextLengthW(edit);
  if (length <= 0 || length > 32767) {
    settings_ui::SetFieldError(edit, true);
    MessageBoxW(materials_window_, Tr(L"请先浏览文件或输入完整路径。"),
                Tr(L"素材导入"), MB_OK);
    SetFocus(edit);
    SendMessageW(edit, EM_SETSEL, 0, -1);
    return;
  }
  settings_ui::SetFieldError(edit, false);
  std::vector<wchar_t> buffer(static_cast<size_t>(length) + 1);
  GetWindowTextW(edit, buffer.data(), static_cast<int>(buffer.size()));
  std::wstring source(buffer.data());
  if (source.size() > 1 && source.front() == L'"' && source.back() == L'"') {
    source = source.substr(1, source.size() - 2);
  }
  if (!SetTimer(window_, kImportTimer, 50, nullptr)) {
    MessageBoxW(materials_window_, Tr(L"无法初始化导入状态，请重试。"),
                Tr(L"素材导入"), MB_OK);
    return;
  }
  try {
    pending_style_index_ = -1;
    import_future_ = std::async(std::launch::async,
                                [source]() { return ImportStyleFile(source); });
    LoadControls();
    SetDlgItemTextW(materials_window_, kSelectedFile,
                    Tr(L"正在验证并导入素材…"));
  } catch (const std::exception&) {
    KillTimer(window_, kImportTimer);
    MessageBoxW(materials_window_, Tr(L"无法启动导入任务，请重试。"),
                Tr(L"素材导入"), MB_OK);
  }
}

void SettingsWindow::BeginSelectStyle(size_t index) {
  if (import_future_.valid() || index >= original_.user_styles.size()) return;
  const auto relative_path = original_.user_styles[index].relative_path;
  if (!SetTimer(window_, kImportTimer, 50, nullptr)) {
    MessageBoxW(window_, Tr(L"无法初始化素材加载，请重试。"), Tr(L"样式选择"),
                MB_OK);
    LoadControls();
    return;
  }
  try {
    pending_style_index_ = static_cast<int>(index);
    import_future_ = std::async(std::launch::async, [relative_path]() {
      StyleImportResult result;
      result.relative_path = relative_path;
      const auto full =
          std::filesystem::path(ApplicationDirectory()) / relative_path;
      result.asset = LoadStyleFile(full.wstring(), &result.error);
      return result;
    });
    LoadControls();
  } catch (const std::exception&) {
    KillTimer(window_, kImportTimer);
    LoadControls();
    MessageBoxW(window_, Tr(L"无法启动素材加载，请重试。"), Tr(L"样式选择"),
                MB_OK);
  }
}

void SettingsWindow::FinishImport() {
  // Defer naming/error dialogs until the settings window is restored.
  if (!IsWindowVisible(window_) || IsIconic(window_)) return;
  if (!import_future_.valid() || import_future_.wait_for(std::chrono::seconds(
                                     0)) != std::future_status::ready)
    return;
  KillTimer(window_, kImportTimer);
  StyleImportResult result;
  try {
    result = import_future_.get();
  } catch (const std::exception&) {
    result.error = Tr(L"导入任务失败。");
  }
  // Preserve any numeric edits made while the asynchronous task was running.
  ReadControls(false);
  if (result.asset) {
    bool use_style = pending_style_index_ >= 0;
    if (!use_style) {
      StyleNameDialog data{&draft_.user_styles, result.original_filename, {}};
      naming_style_ = true;
      const auto choice = DialogBoxParamW(
          GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_STYLE_NAME),
          materials_window_ ? materials_window_ : window_, StyleNameProc,
          reinterpret_cast<LPARAM>(&data));
      naming_style_ = false;
      if (choice == IDOK) {
        draft_.user_styles.push_back(
            {std::move(data.name), result.relative_path});
        use_style = true;
      } else {
        // This is a new private copy, never a previously saved style file.
        const auto full = std::filesystem::path(ApplicationDirectory()) /
                          result.relative_path;
        DeleteFileW(full.c_str());
        if (choice == -1)
          MessageBoxW(materials_window_ ? materials_window_ : window_,
                      Tr(L"无法打开命名窗口，请重新导入。"), Tr(L"素材导入"),
                      MB_ICONERROR);
      }
    }
    if (use_style) {
      draft_.style_file = std::move(result.relative_path);
      draft_.image_asset = std::move(result.asset);
      preview_started_ = GetTickCount64();
      draft_.mode = draft_.image_asset->is_gif ? IndicatorMode::kGif
                                               : IndicatorMode::kImage;
    }
  } else {
    if (pending_style_index_ < 0 && materials_window_)
      settings_ui::SetFieldError(
          GetDlgItem(materials_window_, kSourcePath), true);
    MessageBoxW(
        materials_window_ ? materials_window_ : window_,
        Tr(result.error.c_str()),
        pending_style_index_ < 0 ? Tr(L"素材导入失败") : Tr(L"样式加载失败"),
        MB_ICONWARNING);
  }
  LoadControls();
  if (!materials_window_) PreviewDraft();
}
}  // namespace mouse_cursor_detector
