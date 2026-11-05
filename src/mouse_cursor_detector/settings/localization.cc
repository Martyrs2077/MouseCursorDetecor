#include "mouse_cursor_detector/settings/localization.h"

#include <atomic>
#include <cwchar>

namespace mouse_cursor_detector {
namespace {
std::atomic<UiLanguage> language{UiLanguage::kChinese};
struct Translation {
  const wchar_t* chinese;
  const wchar_t* english;
};
// Both language configurations are built into the portable executable.
constexpr Translation kTranslations[] = {
    {L"为光圈样式命名", L"Name this style"},
    {L"确定", L"OK"},
    {L"取消导入", L"Cancel"},
    {L"请输入样式名称（区分大小写，不允许完全重名）。\n留空使用原文件名：",
     L"Enter a style name (case-sensitive; duplicates are not allowed).\nLeave "
     L"blank to use the filename: "},
    {L"该样式名称已存在，请输入其他名称后再确定。",
     L"This style name already exists. Enter another name."},
    {L"名称须为 1–255 个字符，且不能含控制字符。",
     L"Use 1–255 characters without control characters."},
    {L"样式名称不可用", L"Style name unavailable"},
    {L"鼠标提示设置", L"Mouse Indicator Settings"},
    {L"无法创建设置窗口。", L"Unable to create the settings window."},
    {L"鼠标提示", L"Cursor Cue"},
    {L"提示设置", L"Indicator"},
    {L"素材库", L"Materials"},
    {L"外观主题", L"Appearance"},
    {L"极简聚焦", L"Minimal Focus"},
    {L"清晰原生", L"Native Clarity"},
    {L"暗色工作台", L"Dark Workspace"},
    {L"纸感仪表", L"Paper Dashboard"},
    {L"运行中", L"Active"},
    {L"提示外观", L"Appearance"},
    {L"光圈提示", L"Ring"},
    {L"高亮光标", L"Highlight"},
    {L"导入图片 / GIF", L"Import image / GIF"},
    {L"当前样式", L"Style"},
    {L"颜色", L"Color"},
    {L"选择颜色", L"Choose color"},
    {L"半径", L"Radius"},
    {L"线宽", L"Width"},
    {L"不透明度", L"Opacity"},
    {L"实时更新", L"Live update"},
    {L"触发方式", L"Trigger"},
    {L"更改", L"Change"},
    {L"快捷键输入框初始化失败，请关闭设置后重试。",
     L"Unable to initialize the hotkey field. Close settings and try again."},
    {L"保持时长", L"Duration"},
    {L"秒", L"s"},
    {L"再次触发会重新计时；长按只触发一次",
     L"Retrigger to restart; holding fires once"},
    {L"开机自启动", L"Run at login"},
    {L"登录 Windows 后自动运行", L"Start with Windows"},
    {L"恢复默认", L"Defaults"},
    {L"取消", L"Cancel"},
    {L"保存设置", L"Save settings"},
    {L"半尺寸：8–256 DIP；时长：1–3600 秒",
     L"Half-size: 8–256 DIP; duration: 1–3600 s"},
    {L"半径：8–100 DIP；时长：1–3600 秒",
     L"Radius: 8–100 DIP; duration: 1–3600 s"},
    {L"半尺寸", L"Half size"},
    {L"保留系统光标 · 设置保存在 config/settings.json",
     L"Settings: config/settings.json"},
    {L"自定义素材", L"Custom material"},
    {L"  ·  等比预览", L"  ·  Scaled preview"},
    {L"自启动路径已变化，请点击修正", L"Click to repair startup path"},
    {L"请输入范围内的数值；保持时长为 1–3600 秒，光圈线宽须小于半径的两倍。",
     L"Enter values within range. Duration: 1–3600 seconds; ring width must be "
     L"less than twice the radius."},
    {L"设置未保存", L"Settings not saved"},
    {L"请先在导入光圈样式中导入适用的图片或 GIF。",
     L"Import a compatible image or GIF in Materials first."},
    {L"尚无可用素材", L"No material available"},
    {L"请点击快捷键输入框，按下单键或组合键。",
     L"Click the hotkey field and press a key or key combination."},
    {L"设置已保存。", L"Settings saved."},
    {L"保存成功", L"Saved"},
    {L"正在导入素材，请稍候再关闭设置。",
     L"Importing material. Please wait before closing settings."},
    {L"素材导入", L"Material import"},
    {L"图片与 "
     L"GIF\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.ico;*.gif;*.jxr;*.webp;*."
     L"heic;*.avif\0所有文件\0*.*\0",
     L"Images and "
     L"GIF\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.ico;*.gif;*.jxr;*.webp;*."
     L"heic;*.avif\0All files\0*.*\0"},
    {L"样式库最多保存 512 个素材。",
     L"The library can contain up to 512 materials."},
    {L"请先浏览文件或输入完整路径。",
     L"Browse for a file or enter its full path first."},
    {L"无法初始化导入状态，请重试。",
     L"Unable to initialize import status. Try again."},
    {L"正在验证并导入素材…", L"Validating and importing…"},
    {L"无法启动导入任务，请重试。", L"Unable to start the import. Try again."},
    {L"无法初始化素材加载，请重试。",
     L"Unable to initialize material loading. Try again."},
    {L"样式选择", L"Select style"},
    {L"无法启动素材加载，请重试。",
     L"Unable to start material loading. Try again."},
    {L"导入任务失败。", L"The import task failed."},
    {L"无法打开命名窗口，请重新导入。",
     L"Unable to open the naming dialog. Import the material again."},
    {L"素材导入失败", L"Material import failed"},
    {L"样式加载失败", L"Style loading failed"},
    {L"无法创建导入光圈样式窗口。",
     L"Unable to create the material import page."},
    {L"导入新素材", L"Import new material"},
    {L"浏览", L"Browse"},
    {L"导入素材", L"Import"},
    {L"素材预览", L"Preview"},
    {L"等待导入", L"Awaiting import"},
    {L"最大 20 MiB · 导入后可命名并预览", L"Max 20 MiB · Name and preview"},
    {L"素材会复制到 styles/，原文件可删除",
     L"Copied to styles/; the original can be deleted"},
    {L"保存样式", L"Save styles"},
    {L"选择文件，或粘贴完整路径", L"Choose a file, or paste its full path"},
    {L"正在导入素材，请稍候再关闭。",
     L"Importing material. Please wait before closing."},
    {L"导入光圈样式", L"Import style"},
    {L"未选择样式", L"No style selected"},
    {L"尚未导入素材。\n支持 PNG、JPG、BMP、GIF 等格式。",
     L"No material imported.\nSupports PNG, JPG, BMP, GIF and more."},
    {L"导入后可在此预览", L"Preview appears after import"},
    {L"GIF 动画", L"Animated GIF"},
    {L"静态图片", L"Static image"},
    {L" 像素\n", L" pixels\n"},
    {L" 帧", L" frames"},
    {L"当前素材", L"Current material"},
    {L"正在导入…", L"Importing…"},
    {L"已载入", L"Loaded"},
    {L"把喜欢的图片或 GIF 变成提示样式",
     L"Turn an image or GIF into an indicator"},
    {L"已保存 ", L"Saved: "},
    {L" 个", L""},
    {L"透明背景", L"Transparent"},
    {L"当前选择", L"Selected"},
    {L"保存后可在主设置的“当前样式”中选择。\n删除文件前请先切换。",
     L"After saving, select it under Style in Indicator.\nSwitch styles before "
     L"deleting files."},
    {L"实时预览", L"Preview"},
    {L"同步", L"Live"},
    {L"点击穿透 · 不抢焦点", L"Click-through · No focus"},
    {L"导入后在此预览", L"Import to preview here"},
    {L"图片", L"Image"},
    {L"高亮", L"Highlight"},
    {L"光圈", L"Ring"},
    {L" 秒后隐藏", L" s until hidden"},
    {L"云杉绿 · 清晰直接", L"Spruce green · Clear and direct"},
    {L"石墨铜 · 低光专注", L"Graphite copper · Low-light focus"},
    {L"纸莎棕 · 温暖手感", L"Papyrus brown · Warm feel"},
    {L"石墨蓝 · 居中聚焦", L"Graphite blue · Centered focus"},
    {L"原光标不变", L"Cursor intact"},
    {L"只叠加提示层，", L"An overlay only;"},
    {L"不替换系统指针。", L"your cursor stays."},
    {L"已保存：", L"Saved: "},
    {L"尚未保存 · 上次保存：", L"Unsaved · Last saved: "},
    {L"（已选择）", L" (selected)"},
    {L"正在处理素材，请完成后再切换页面。",
     L"Material processing is in progress. Finish it before switching pages."},
    {L"四种气质，一套操作", L"Four looks, one workflow"},
    {L"让指针在屏幕上被看见", L"Keep your pointer in sight"},
    {L"当前：", L"Theme: "},
    {L"实时应用", L"Live"},
    {L"全局监听", L"Global"},
    {L"系统", L"System"},
    {L"提示显示状态仅本次运行有效", L"Visibility lasts only for this session"},
    {L"当前", L"Active"},
    {L"MouseCursorDetector 0.1.0\n\n透明底色 / "
     L"空心光圈；不替换、不隐藏系统光标。\n默认同时按下逗号键和句号键触发提示，"
     L"3 秒后关闭；时长可修改。\n\n参考 MAXsCursor "
     L"的原生窗口与跟随线程设计。\n\n",
     L"MouseCursorDetector 0.1.0\n\nTransparent highlight / hollow ring; the "
     L"system cursor stays unchanged.\nPress comma and period together to show "
     L"the indicator. It hides after 3 seconds by default; duration is "
     L"configurable.\n\nNative window and tracking thread design inspired by "
     L"MAXsCursor.\n\n"},
    {L"关于 / 开源许可", L"About / Open-source licenses"},
    {L"无法创建托盘图标。程序将退出。",
     L"Unable to create the tray icon. The application will exit."},
    {L"键盘输入监听初始化失败，请退出后重试。",
     L"Unable to initialize keyboard input. Exit and try again."},
    {L"提示线程初始化失败。可退出后重试。",
     L"Unable to initialize the indicator thread. Exit and try again."},
    {L"设置无法读取或版本不受支持，本次使用默认值。",
     L"Settings could not be read or use an unsupported version. Defaults are "
     L"used for this session."},
    {L"无法创建 config/settings.json，请检查软件目录写入权限。",
     L"Unable to create config/settings.json. Check write access to the "
     L"application folder."},
    {L"自启动路径与当前位置不同，请在设置中修正。",
     L"The startup path differs from the current location. Repair it in "
     L"settings."},
    {L"\n错误 ", L"\nError "},
    {L"提示线程不可用，请退出后重新启动。",
     L"The indicator thread is unavailable. Exit and restart the application."},
    {L"无法设置自动关闭计时器，提示未开启。",
     L"Unable to set the auto-hide timer. The indicator was not enabled."},
    {L"无法更新提示状态。", L"Unable to update the indicator state."},
    {L"键盘输入监听初始化失败，设置未保存。",
     L"Unable to initialize keyboard input. Settings were not saved."},
    {L"设置未能保存，窗口将保持打开。",
     L"Unable to save settings. The window will remain open."},
    {L"原快捷键恢复失败，请重新选择快捷键。",
     L"Unable to restore the original hotkey. Choose the hotkey again."},
    {L"无法读取登录自启动设置。", L"Unable to read the run-at-login setting."},
    {L"自启动设置未能更新。", L"Unable to update the run-at-login setting."},
    {L"已登记登录自启动。如果 Windows 启动应用中被禁用，请在那里启用。",
     L"Run at login is registered. If disabled in Windows Startup apps, enable "
     L"it there."},
    {L"托盘恢复失败，请退出后重试。",
     L"Unable to restore the tray icon. Exit and try again."},
    {L"提示层已停止；请退出后重新启动程序。",
     L"The indicator has stopped. Exit and restart the application."},
    {L"鼠标提示 · 等待快捷键", L"Mouse Indicator · Awaiting hotkey"},
    {L"鼠标提示 · %ls · %ls", L"Mouse Indicator · %ls · %ls"},
    {L"正在显示", L"Visible"},
    {L"等待快捷键", L"Awaiting hotkey"},
    {L"打开设置菜单", L"Open settings"},
    {L"关于", L"About"},
    {L"退出", L"Exit"},
    {L"程序初始化失败或内存不足。",
     L"Application initialization failed or memory is insufficient."},
    {L"无法读取素材，格式不受支持、文件损坏或超出限制。",
     L"Unable to read the material: unsupported format, damaged file, or "
     L"limits exceeded."},
    {L"素材不存在、无法读取或大小超过 20 MiB。",
     L"The material is missing, unreadable, or larger than 20 MiB."},
    {L"素材解码失败或超限：静态图最大 4096×4096；GIF 最大 1024×1024、200 "
     L"帧、64 Mi 像素处理量及 64 MiB 帧缓存。",
     L"Decoding failed or limits exceeded: images up to 4096×4096; GIFs up to "
     L"1024×1024, 200 frames, 64 Mi processed pixels and 64 MiB frame cache."},
    {L"素材处理失败，文件无效或可用内存不足。",
     L"Material processing failed: invalid file or insufficient memory."},
    {L"无法确定软件目录。", L"Unable to locate the application folder."},
    {L"请选择有效图片，文件大小不能超过 20 MiB。",
     L"Choose a valid image no larger than 20 MiB."},
    {L"无法创建 styles 文件夹，请检查软件目录写入权限。",
     L"Unable to create the styles folder. Check write access to the "
     L"application folder."},
    {L"无法生成素材文件名。", L"Unable to generate a material filename."},
    {L"无法复制素材，请检查源文件与软件目录写入权限。",
     L"Unable to copy the material. Check the source and write access to the "
     L"application folder."},
    {L"导入失败，请检查文件路径和可用空间。",
     L"Import failed. Check the file path and available space."},
    {L"配置中的素材路径无效，已回退到内置光圈。",
     L"The configured material path is invalid. The built-in ring is used "
     L"instead."},
    {L"所选素材不可用，已回退到内置光圈。",
     L"The selected material is unavailable. The built-in ring is used "
     L"instead."},
};
}  // namespace

void SetUiLanguage(UiLanguage value) {
  language.store(value, std::memory_order_relaxed);
}

UiLanguage CurrentUiLanguage() {
  return language.load(std::memory_order_relaxed);
}

const wchar_t* Tr(const wchar_t* chinese) {
  if (CurrentUiLanguage() != UiLanguage::kEnglish) return chinese;
  for (const auto& entry : kTranslations) {
    if (std::wcscmp(chinese, entry.chinese) == 0) return entry.english;
  }
  return chinese;
}
}  // namespace mouse_cursor_detector
