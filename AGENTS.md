# AGENTS.md — MouseCursorDetector 项目指南

本文件是本目录的助手工作约定。开始修改前请完整阅读；它约束的是“如何工作”，不替代用户当次的任务要求；第 2 节的硬性约束只有用户明确撤销时才可放宽。凡涉及构建、光标、运行时数据或发布，先核对本文件。

## 1. 项目概览

MouseCursorDetector 是 Windows 10/11 x64 的鼠标位置提示工具。默认同时按下 `,` 和 `.` 触发提示，保持 3 秒后自动隐藏；支持内置半透明高亮、空心光圈、自定义图片/GIF、四套界面主题、可配置快捷键与保持时长、托盘驻留及当前用户登录自启动。

- 技术栈：C++17、Win32、GDI+、WIC、CMake + Ninja、MSYS2 UCRT64 GCC。
- 运行形态：一个 `MouseCursorDetector.exe`；GCC 运行库静态链接，不携带第三方 DLL 或辅助 EXE。
- 用户数据：EXE 同目录的 `config/settings.json` 与 `styles/`；路径以 EXE 所在目录为基准，不以终端当前目录或源码目录为基准。
- 发布形态：根目录便携 EXE，以及 `MouseCursorDetector-Setup.exe` 当前用户安装包。
- 所有用户可见文字使用中文；代码标识符、注释和文档可按现有文件风格使用英文或中文。

## 2. 硬性约束（最高优先级）

### 2.1 默认手动构建，明确推送时由助手先构建

- 默认编译由用户手动执行。没有明确推送请求时，助手可以维护源码、构建脚本及配置，但不得自行运行 CMake 配置、构建、编译、链接或通过任务间接触发这些操作。
- 2026-11-05 用户增加明确例外：用户要求“推送”“推送至远端”“提交并推送”等实际推送操作时，助手必须先运行项目现有 `scripts/build.ps1`（即 `build.cmd` 调用的实际编译文件），成功后将相关改动和根目录两个 EXE 提交，再推送 `Dev`，无需重复确认构建和提交授权。构建、依赖检查、打包或根目录产物复制失败时，停止提交和推送，不以旧产物继续。
- 此例外仅允许通过现有构建脚本完成编译与打包，不授权安装、运行产品、发布 Release 或自动升级版本。用户另行明确限定不构建或不提交工作区时，以当次要求为准。
- 用户确认功能、要求仅本地提交、拉取更新、维护技能或讨论未来推送规则，都不构成本次构建授权；本次是否构建取决于实际推送请求，不能只匹配“推送”字样。
- 静态阅读、JSON / PowerShell 语法解析和 clang-format 检查可以执行，不得借此调用编译器。
- C++ 代码遵守 Google C++ Style Guide。

未满足上述推送授权条件时，助手不得运行：

- `build.cmd`、`scripts/build.ps1` 或 VS Code 的 `GCC: Build EXE and installer` 任务；
- `cmake --preset ...`、`cmake --build ...`、`ninja`、`g++.exe`、`windres.exe`、`ISCC.exe`；
- 任何通过脚本、管道、后台任务或子进程间接触发上述工具的等价命令。

单纯为“检查能否编译”或“验证改动”不自动获得构建授权。静态检查、编译结果和交互验收必须分别报告，构建成功不等于功能验收通过。

### 2.2 不得改变系统鼠标光标

- 保持现有系统鼠标光标不变。不得修改、替换、隐藏或恢复光标方案、光标文件、系统光标注册表或其他应用的光标。
- 不使用 `SetSystemCursor`、`SetCursor`、`ShowCursor` 等 API 替换或隐藏指针；不在屏幕上绘制会替代系统指针的图形。
- 提示层只是独立的透明分层窗口，原有箭头、文本插入指针、手形、缩放及忙碌指针由系统或目标应用照常管理。
- 程序正常退出或异常终止后都不需要恢复系统光标。

### 2.3 保持便携单 EXE 形态

- 用户已确认可采用单 EXE 加 `config/settings.json` 和 `styles/` 用户素材目录；路径以 EXE 所在目录为基准。
- 不释放辅助 EXE、脚本、日志或第三方 DLL；系统 DLL 由 Windows 提供。
- 不为运行时引入新的第三方库或动态依赖；新增依赖前必须先获得用户确认。

## 3. 当前状态与验证边界

- 源码、构建脚本、安装脚本、设计文档和静态审查记录均已存在；根目录已有构建产物。
- 项目文档仍将编译后的功能验收标记为未完成。产物存在、静态检查通过或代码审阅通过，都不能表述为“已运行验证”。
- 项目没有自动化测试套件；验证以构建后的人工交互验收为主，不得虚构构建、测试或运行结果。
- 助手修改源码后，按实际说明静态检查、构建和运行验收状态。未构建时说明“尚未编译、尚未运行验证”并列出手动构建命令；因明确推送请求已构建成功时，报告真实构建结果和仍需用户完成的交互验收，不要求重复构建。
- 除非用户明确要求，不启动根目录 EXE、安装包或 `build/` 中的产物；运行会产生或修改用户配置与素材。
- 2026-11-05 用户明确要求根目录 `MouseCursorDetector.exe` 和 `MouseCursorDetector-Setup.exe` 随源码提交，并将现有两个产物上传 GitHub Release。该次上传现有产物的授权本身不包含构建、运行或安装；后续明确推送时的构建例外见第 2.1 节。不得擅自删除或重置这两个文件。`config/settings.json` 和用户导入素材仍保留在本地。

## 4. 构建与发布

普通使用者可直接下载 GitHub Release 中的便携 EXE 或安装包。`build.cmd`、VS Code 构建任务和 `scripts/build.ps1` 供开发者编译使用，不是普通使用者运行软件的前置步骤。默认由用户手动构建；明确要求助手推送时，助手先直接运行 `scripts/build.ps1` 构建，成功后提交源码及根目录两个 EXE，再推送。发布 Release 仍须有用户的明确发布要求；构建与推送授权不包含产品运行或安装。

工具链固定为 `C:\GCC\ucrt64\bin`，Preset 为 `gcc-debug` / `gcc-release`。用户可任选一种方式手动构建：

1. 双击根目录 `build.cmd`；
2. 在 VS Code 中按 `Ctrl+Shift+B`，运行 `GCC: Build EXE and installer`；
3. 在项目根目录执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

`scripts/build.ps1` 会使用 Release preset 构建，检查 x64 PE 与 DLL 导入，拒绝 GCC/MSYS 动态运行库和非系统依赖，再调用 Inno Setup 生成安装包，最后把两个产物复制到项目根目录。首次构建会下载并校验 Inno Setup 6.7.4。

产物位置：

- `build/gcc-release/MouseCursorDetector.exe`：构建中间产物；
- `build/installer/MouseCursorDetector-Setup.exe`：安装包中间产物；
- 根目录 `MouseCursorDetector.exe`、`MouseCursorDetector-Setup.exe`：最终交付物；
- `dist/`：旧版发布目录，保留但不再作为当前流程的更新目标。

仅修改构建脚本、CMake 配置或安装脚本时，先做静态审阅和语法检查；没有明确推送请求时提醒用户手动构建，明确推送时按第 2.1 节执行构建后再提交推送。

## 5. 目录与模块边界

```text
src/mouse_cursor_detector/
  program.cc                 wWinMain、单实例、GDI+ 生命周期
  app.h / app.cc             主线程编排、计时、预览/保存、托盘与设置窗口
  core/
    hook_manager.*           钩子线程、鼠标钩子、光圈窗口所有权、DPI 与维护
    hotkey_manager.*         Raw Input 快捷键监听与绑定替换
    startup_manager.*        HKCU Run 登录自启动
  overlay/
    native_cursor_window.*   WS_EX_LAYERED 透明窗口、GDI+ 绘制、位图上传
  settings/
    settings_model.h         设置类型、默认值与参数归一化
    settings_store.*         旧版注册表读取与迁移
    portable_settings.*      config/settings.json 读写、样式库路径校验
    settings_window.*        主设置窗口、三页导航、快捷键捕获
    settings_themes.*        四套主题的配色与绘制
    settings_materials.*     素材导入页面、命名与预览
    settings_ui.h            共用控件绘制与布局辅助
    settings_control_frames.h  控件外框/焦点绘制
    settings_control_ids.h   控件 ID
  media/image_asset.*        WIC 图片/GIF 解码与帧合成
  tray/tray_icon.*           托盘图标与菜单
  interop/window_messages.h  自定义窗口消息
  resources/                 清单、图标、版本、许可
```

模块边界：

- 主线程负责控制窗口、托盘、快捷键、设置窗口和配置文件；钩子线程负责鼠标钩子、提示窗口、跟随移动、DPI 切换和动画。
- `NativeCursorWindow` 的所有操作（包括销毁）都属于钩子线程；主线程不要直接调用它，统一通过 `HookManager::ApplyState` 投递设置快照。
- 鼠标回调只做位置记录、窗口移动和 `CallNextHookEx`；不得在回调中分配内存、绘图、读写文件/注册表、重建 DPI 资源或同步等待 UI。
- 跨线程通信用 `PostMessage` 和受短锁保护的最新快照，不跨线程 `SendMessage`；连续样式更新应合并。
- 素材导入在 `SettingsWindow` 的异步任务中完成，UI 只轮询结果；钩子线程不参与解码或文件 I/O。
- 设置窗口内部按主设置、素材导入、主题选择三页组织；主题只影响界面，不改变系统光标或提示参数。
- 新增 `.cc` 文件必须同步加入 `CMakeLists.txt`；源码列表是显式的，不依赖通配符。

## 6. 线程、生命周期与绘制约定

启动顺序：单实例 → 加载并校验设置 → 创建控制窗口和托盘 → 启动键盘监听 → 钩子线程初始化 → 报告初始化成功 → 提示保持隐藏，等待用户触发。

退出顺序：停止接收新命令 → 停止快捷键 → 通知钩子线程退出 → 线程内移除钩子/事件/定时器并销毁提示窗口和 DIB → 等待线程真正结束 → 清理托盘和控制窗口。不要用固定超时代替实际线程结束。

绘制与移动：

- 高亮为半透明实心圆盘，光圈为中心透明的空心圆环；两者互斥，切换时先清空缓冲区，避免残影。
- 只在模式、颜色、半径、线宽、不透明度或 DPI 变化时重绘并 `UpdateLayeredWindow`；普通鼠标移动只 `SetWindowPos`。
- 提示窗口使用 `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW`，点击穿透、不抢焦点、不出现在任务栏。
- 尺寸以 DIP 配置、按目标显示器 DPI 换算；支持多屏、负坐标和混合缩放，不假设启动时 DPI 永远不变。
- 内置高亮/光圈保持静态外观，只有 GIF 素材按帧播放；不新增淡出、波纹、放大镜、HUD 等未被要求的效果。

## 7. 运行时数据与兼容性

- `config/settings.json` 为版本 4 的平面 JSON；兼容读取版本 1/2/3。当前选择保存 `style_name`；每素材图片中心相对指针热点的 DIP 偏移独立保存在配置的 `image_offset_*` 平面字段（坐标加 512 编码为无符号整数），缺省为零；唯一素材名称到文件映射位于 `styles/library.json`（版本 1 平面 JSON）。值为无符号整数或字符串，不接受注释、重复字段、数组或嵌套对象，配置文件上限 2 MiB。
- JSON 通过同目录临时文件写入、刷新后替换；保存失败不得破坏旧配置，也不得自动覆盖损坏文件。
- 没有 JSON 时，才从 `HKCU\Software\MouseCursorDetector` 读取旧版 `REG_BINARY` 并迁移；迁移后以 JSON 为准，旧记录只读，不再回写。
- 自启动仍使用 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 的 `MouseCursorDetector` 值，路径为带引号的当前 EXE 绝对路径加 `--autostart`。
- 提示显示状态仅在本次运行有效，不持久化；每次启动默认隐藏并继续监听。
- 素材文件复制到 EXE 旁的 `styles/`，保留原文件名，重名按扩展名前 `_1`、`_2` 递增；映射表只保存 `styles/文件名.ext` 形式的相对路径，普通配置仅保存所选名称；拒绝绝对路径、`..` 跳转、嵌套目录和同名显示名称（严格区分大小写）。
- 进入素材库及点击“刷新”会重读映射、清理明确缺失项并登记目录内有效的未映射素材；当前正式素材缺失时恢复默认光圈及数值、保持 3 秒。选择和参数只在窗口内预览，保存配置后才正式应用。样式库最多 512 项，显示名称最多 255 个 UTF-16 字符；素材大小、尺寸、帧数等限制见 `docs/PORTABLE_STYLES.md`。
- `config/`、`styles/`、`build/`、`dist/`、`.tools/`、`.reference/` 均属于本地或生成内容，不得当作源码提交。经用户明确授权，根目录 `MouseCursorDetector.exe` 和 `MouseCursorDetector-Setup.exe` 作为交付物纳入版本控制。

默认值（修改默认行为前先确认这是用户要求）：

| 项目 | 默认值 |
| --- | --- |
| 界面主题 | 3，极简聚焦 |
| 提示模式 | 1，光圈 |
| 触发键 | `VK_OEM_COMMA` + `VK_OEM_PERIOD`，顺序不限 |
| 保持时长 | 3 秒，允许 1–3600 秒 |
| 启动时显示 | 否 |
| 光圈颜色/半径/线宽/不透明度 | `#02FF60` / 36 DIP / 7 DIP / 90% |
| 高亮颜色/半径/不透明度 | `#FFFF00` / 24 DIP / 25% |

## 8. 代码规范

- 遵循 Google C++ Style Guide，并以根目录 `.clang-format` 为准：Google 预设、80 列、指针左对齐、保留 include 分块。
- 文件使用小写加下划线，头文件用 `.h`、实现用 `.cc`；类型用 PascalCase，函数用 PascalCase，局部变量和成员用 snake_case，成员变量带尾随下划线，常量用 `k` 前缀。
- 项目代码通常位于 `mouse_cursor_detector` 命名空间（`wWinMain` 入口等 Win32 边界除外）；头文件使用完整路径形式的 include guard，例如 `MOUSE_CURSOR_DETECTOR_CORE_HOOK_MANAGER_H_`。
- include 顺序保持现有风格：`<windows.h>` 等系统头 → 标准库 → 本项目头文件；不要用 `using namespace`。
- 使用 C++17 和 Unicode Win32 API（`wchar_t`、`W` 后缀），不引入 MFC、Qt、WPF 或新的框架。
- Win32/GDI+/WIC 资源使用 RAII 或成对的创建/释放函数；检查 `HRESULT`、`LSTATUS`、`BOOL` 返回值，失败时保留可操作状态并报告错误。
- 不要在窗口过程或钩子回调中让异常逸出；入口处已有兜底异常处理，新增代码应优先返回错误码。
- 新增界面颜色、圆角、边框、字体和 DPI 换算时，优先复用 `settings_ui.h` / `settings_themes.*` 的现有抽象，不要在多个文件散落魔法值。
- 保留原生控件的 Tab、Enter、Space、IME、选择与剪贴板语义；自绘只补充视觉，不替换控件行为。
- 修改设置模型、JSON 字段或默认值时，必须同时考虑旧配置读取、`Normalize()` 边界和保存回读。

## 9. 修改检查清单

提交改动前逐项确认：

- [ ] 没有引入任何替换、隐藏或修改系统光标的代码。
- [ ] 未在明确推送授权之外运行或间接触发编译、链接、打包；未安装或运行产品。推送前构建成功，失败时未继续提交或推送。
- [ ] 新增源文件已加入 `CMakeLists.txt`，没有引入新的运行时 DLL 或第三方库。
- [ ] 设置默认值、JSON 版本、旧版注册表迁移和相对路径规则未被无意改变。
- [ ] 钩子线程仍拥有提示窗口；鼠标回调仍无阻塞 I/O、绘图和同步等待。
- [ ] 界面仍支持四套主题、DPI 缩放、键盘操作和点击穿透。
- [ ] 已做相应静态检查（JSON/PowerShell 语法、clang-format、静态阅读），如实区分静态检查、构建结果和运行验收；未构建或未运行时明确说明。
- [ ] 行为或配置变化已同步到 `README.md`、`DESIGN.md`、`ARCHITECTURE.md` 或 `docs/` 中对应文档。
- [ ] 最终回复列出仍需用户执行的交互验收项；未构建时附手动构建命令，已构建成功时报告构建结果而不要求重复构建。

## 10. 版本控制边界

2026-11-05 经用户确认，本目录已建立独立 Git 仓库，Git 根目录必须为本项目目录。同步目标为 `Martyrs2077/MouseCursorDetecor` 的 `Dev` 分支；不得在 `C:\Users\25036` 的上级仓库中暂存、提交或推送本项目。

源码、文档、构建配置、可共享的项目技能和根目录两个交付物 `MouseCursorDetector.exe`、`MouseCursorDetector-Setup.exe` 纳入版本控制；`config/`、`styles/`、`build/`、`dist/`、`.tools/`、`.reference/` 和 `design-previews/` 保留在本地，不提交。`.agents/skills/github-sync/` 是本地专用技能，整个目录不纳入版本控制，不得随代码提交或推送；该限制不影响技能执行用户授权的其他代码提交和推送。首次覆盖远端分支已获用户明确授权，后续同步仍默认普通推送；再次重写远端历史需另行获得用户明确授权。

默认运行及构建所需素材必须随源码提交，当前包括 `src/mouse_cursor_detector/resources/` 中的图标、清单和许可。默认光圈与高亮由代码绘制，无外部素材依赖；`config/settings.json` 当前没有引用自定义素材。后续若增加文件形式的默认素材，应同步提交其文件并核对配置引用完整性。

## 11. 文档导航

| 文档 | 用途 |
| --- | --- |
| [README.md](README.md) | 功能范围、构建步骤、安装与验收清单 |
| [DESIGN.md](DESIGN.md) | 四套主题、控件、边框、按钮和输入框设计决定 |
| [ARCHITECTURE.md](ARCHITECTURE.md) | 模块职责、线程模型、绘制、DPI 与持久化设计 |
| [ENVIRONMENT_CHECKLIST.md](ENVIRONMENT_CHECKLIST.md) | 工具链安装与检查记录 |
| [docs/PORTABLE_STYLES.md](docs/PORTABLE_STYLES.md) | 自定义图片/GIF、JSON 字段和素材限制 |
| [docs/FLICKER_QA.md](docs/FLICKER_QA.md)、[docs/LAYOUT_QA.md](docs/LAYOUT_QA.md) | 闪烁与布局的静态核查记录 |
| [docs/THEME_ACCEPTANCE.md](docs/THEME_ACCEPTANCE.md) | 主题验收项 |

`.reference/MAXsCursor/` 是固定提交的只读参考材料，采用 MIT 许可，不参与构建。若实质复制或改写其中的代码，必须保留版权与许可声明，并同步更新 `src/mouse_cursor_detector/resources/third_party_notices.txt`。

## 12. 控件来源与显示层变更

- 本项目每次新增、替换或修改界面控件前，必须读取并执行项目内 skill：[ant-design-controls](.agents/skills/ant-design-controls/SKILL.md)，无需用户再次点名；纯业务逻辑任务不因此触发界面替换。
- 允许读取 Ant Design 中文组件总览及其官方子页面中所有控件、子组件和变体的 `LLMs.md` 说明，不限于总览正文卡片。具体组件必须从页面左侧列表的对应分类和组件选项进入官方页面选取，保留“总览 → 左侧分类 → 组件页面 → 子组件/变体 → LLMs.md”的来源链。
- 每次修改前读取所选控件的官方 `LLMs.md` 正文；本次涉及的组件及依赖文档必须完整阅读，无需每次全量读取网站文档。遵循官方描述与格式排版，仅允许调整尺寸参数。
- 用户已允许按官方描述进行 Win32 原生适配，须明确标注适配方式。这不授权引入 React、新框架或第三方运行依赖，不改变第 2 节硬性约束。
- 控件替换限于页面显示实现及必要的事件转接；项目现有业务核心代码与实现逻辑保持不变，显示与业务严格分离。无法兼容时报告冲突，不通过修改业务逻辑或自行重排控件解决。
