#ifndef MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_WINDOW_H_
#define MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_WINDOW_H_

#include <windows.h>

#include <array>
#include <functional>
#include <future>

#include "mouse_cursor_detector/settings/settings_model.h"
#include "mouse_cursor_detector/settings/settings_store.h"

namespace mouse_cursor_detector {
class SettingsWindow {
 public:
  using Preview = std::function<void(const Settings&)>;
  using Commit = std::function<bool(const Settings&)>;
  using Startup = std::function<void()>;
  SettingsWindow(Preview preview, Commit commit, Startup startup,
                 Preview recover_missing);
  ~SettingsWindow();
  void Show(HWND owner, const Settings& settings);
  void Close();
  bool HandleDialogMessage(MSG* message);
  bool is_open() const { return window_ != nullptr; }
  HWND window() const { return window_; }
  bool IsCapturingHotkey() const;
  void RefreshStartup();

 private:
  enum class Page { kMain, kMaterials, kThemes };
  static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam,
                                     LPARAM lparam);
  static LRESULT CALLBACK HotkeyProc(HWND window, UINT message, WPARAM wparam,
                                     LPARAM lparam, UINT_PTR subclass_id,
                                     DWORD_PTR reference);
  static LRESULT CALLBACK PreviewProc(HWND window, UINT message, WPARAM wparam,
                                      LPARAM lparam, UINT_PTR subclass_id,
                                      DWORD_PTR reference);
  struct PreviewGeometry {
    RECT area{};
    float center_x = 0;
    float center_y = 0;
    float scale = 1;
  };
  PreviewGeometry GetPreviewGeometry(HWND control) const;
  void PreparePreview(HWND control);
  bool RefreshPreviewCursor();
  void DrawPreviewCursor(HDC dc, HWND control);
  void DragPreview(HWND control, POINT point);
  static INT_PTR CALLBACK MaterialsProc(HWND window, UINT message,
                                        WPARAM wparam, LPARAM lparam);
  void CreateControls();
  void LayoutControls();
  void LayoutFooter(int left, int right, int top);
  void ScrollControls(int position, bool horizontal = false);
  void PaintWindow(HDC dc);
  void DrawSettingsButton(const DRAWITEMSTRUCT& item);
  void ApplyTheme();
  void SelectPage(Page page);
  void CloseMaterials(bool save, Page next_page = Page::kMain);
  void DrawThemeCard(const DRAWITEMSTRUCT& item);
  void RefreshPreview();
  void LoadControls();
  void RefreshLanguage();
  void ToggleLanguage();
  bool ReadControls(bool report_error);
  void UpdateHotkeyLabel();
  void Command(int id, int notification);
  void PreviewDraft();
  void BrowseStyle();
  void BeginImport();
  void FinishImport();
  void BeginLibraryRefresh();
  void FinishLibraryRefresh();
  bool MaterialBusy() const {
    return import_future_.valid() || library_future_.valid();
  }
  void BeginSelectStyle(size_t index);
  void ShowMaterials();
  void LayoutMaterials();
  void CreateMaterialFonts();
  void PaintMaterials(HDC dc);
  void LoadMaterialControls();
  void DrawMaterialPreview(const DRAWITEMSTRUCT& item);
  void DrawMaterialListItem(const DRAWITEMSTRUCT& item);
  void DrawPreviewStage(HDC dc, HWND control, const RECT& bounds);
  void SyncSliders();
  void HandleSlider(HWND slider);
  HWND window_ = nullptr;
  HWND materials_window_ = nullptr;
  HWND owner_ = nullptr;
  Settings original_;
  Settings draft_;
  Settings preview_settings_;
  Settings materials_before_;
  Settings materials_previous_preview_;
  Page page_ = Page::kMain;
  Preview preview_;
  Preview recover_missing_;
  Commit commit_;
  Startup startup_;
  bool loading_ = false;
  bool layout_active_ = false;
  bool theme_applied_ = false;
  UiTheme applied_theme_ = UiTheme::kMinimal;
  std::array<bool, 256> captured_keys_{};
  std::future<StyleImportResult> import_future_;
  std::future<StyleLibraryResult> library_future_;
  std::vector<std::shared_ptr<const ImageAsset>> material_thumbnails_;
  std::vector<UserStyle> material_list_entries_;
  int pending_style_index_ = -1;
  bool naming_style_ = false;
  HFONT font_ = nullptr;
  HFONT heading_font_ = nullptr;
  HFONT title_font_ = nullptr;
  HFONT small_font_ = nullptr;
  HFONT number_font_ = nullptr;
  std::array<HFONT, 4> material_fonts_{};
  int scroll_dip_ = 0;
  int horizontal_scroll_dip_ = 0;
  bool startup_enabled_ = false;
  ULONGLONG preview_started_ = 0;
  HCURSOR preview_cursor_ = nullptr;  // Borrowed; never modifies the cursor.
  UINT preview_cursor_dpi_ = 96;
  HWND dragged_preview_ = nullptr;
  POINT drag_origin_{};
  ImageOffset drag_offset_;
};
}  // namespace mouse_cursor_detector

#endif  // MOUSE_CURSOR_DETECTOR_SETTINGS_SETTINGS_WINDOW_H_
