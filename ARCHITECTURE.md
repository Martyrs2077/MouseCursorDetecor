# 鼠标光标高亮工具：基于 MAXsCursor 的架构修订

更新日期：2026-11-02  
状态：首版功能源码已编写，已有首轮构建产物，未运行验证。后续配置与编译仅由用户执行。  
项目目录：`C:\Users\25036\Desktop\Personal_Project\mouse_cursor_detector`

**最新执行顺序：** 功能已确认；编译由用户通过脚本或 VS Code 任务手动执行，助手不得触发构建。使用 VS Code + MSYS2 UCRT64 GCC；用户已授权助手将工具链安装至 C:\GCC，安装和检查记录见 [ENVIRONMENT_CHECKLIST.md](ENVIRONMENT_CHECKLIST.md)。代码采用 Google C++ Style Guide，clang-format 基于 Google 预设。

## 1. 修订结论与范围

首版只实现鼠标位置提示：自定义单键或组合键触发限时跟随的提示效果（默认 3 秒，可设置 1–3600 秒），设置中可选择“高亮光标”或“光圈提示”，支持当前用户登录后自启动，最终交付一个 `MouseCursorDetector.exe`。

当前扩展增加“自定义图片”和“GIF 动画”；用户文件复制至 EXE 旁的 styles，config/settings.json 保存相对路径。普通图片在导入/样式变化时上传；GIF 可见时按帧调度上传。导入在独立异步任务内执行，鼠标回调不解码或读文件。主设置按样式、快捷键、系统选项分组，数值框仅显示数字，DIP / % / 秒通过独立控件显示，滑块与数值框同步。素材路径、导入、透明棋盘格与 GIF 预览位于导入光圈样式二级对话框（settings_materials.cc）；二级窗口保存时仅持久化样式库，不提交主窗口的其他编辑；成功后样式才加入下拉列表，取消恢复进入前的草稿和提示效果。本文关于旧版固定记录的内容仅描述迁移来源；当前使用说明与限制见 [PORTABLE_STYLES.md](docs/PORTABLE_STYLES.md)。MP4 尚未实现。

**硬性约束：不得修改、替换或隐藏电脑中已有的鼠标光标图标。** 不修改光标方案、光标文件、系统光标注册表或其他应用的光标；不使用 SetSystemCursor、SetCursor 或 ShowCursor 来替换或隐藏指针。两种模式都只绘制独立的透明提示窗口，原有箭头、文本插入指针、手形、缩放指针及忙碌动画由系统和目标应用照常管理。程序退出或崩溃后不需要恢复光标图标。

“高亮光标”在本方案中定义为：在鼠标热点周围叠加半透明实心圆形底色，视觉上突出原有指针；不是将箭头本体改色。“光圈提示”定义为：热点周围绘制中心完全透明的空心圆环。两者互斥，切换时复用一个窗口，默认继续采用光圈提示。

采用 MAXsCursor 的模块组织和核心执行方式：**专用鼠标钩子线程拥有原生光圈窗口，移动时只改变窗口位置，样式变化时才重新绘制并上传位图**。设置、托盘及快捷键在主线程处理。

为保持单体运行，使用 C++17 / Win32 转写该设计，静态链接 GCC 的 libgcc、libstdc++ 和所需线程运行库，不直接采用 C# / WPF 发布方式。2026-11-02 用户确认调整便携存储：程序仍为一个 EXE，允许其旁的 config/settings.json 与 styles/ 用户素材；不解压辅助程序或第三方 DLL。设置优先从 JSON 读取，旧注册表仅用于首次迁移。

本次移除光标居中及其快捷键；提示到时直接关闭，不添加淡出动画；不引入参考项目的键盘 HUD、点击波纹、放大镜、截图、安装器或自动更新。

## 2. 源码分析基线

参考仓库：[ziwuxin1/MAXsCursor](https://github.com/ziwuxin1/MAXsCursor)。实际读取的固定提交：

```text
748182434a23a0db275ed74b7d62bed3151daf36
```

只读参考副本在 `.reference/MAXsCursor/`，不参与本项目编译和发布。以下链接均固定到该提交。

| 文件 | 源码确认的职责 |
| --- | --- |
| [Program.cs](https://github.com/ziwuxin1/MAXsCursor/blob/748182434a23a0db275ed74b7d62bed3151daf36/src/MAXsCursor/Program.cs) | STA 入口、会话内互斥量、WPF 启动；重复实例直接退出 |
| [App.xaml.cs](https://github.com/ziwuxin1/MAXsCursor/blob/748182434a23a0db275ed74b7d62bed3151daf36/src/MAXsCursor/App.xaml.cs) | 设置、钩子、光圈、HUD、托盘、快捷键、波纹和放大镜的编排与清理 |
| [HookManager.cs](https://github.com/ziwuxin1/MAXsCursor/blob/748182434a23a0db275ed74b7d62bed3151daf36/src/MAXsCursor/Core/HookManager.cs) | 专用线程、鼠标/键盘低级钩子、光圈直接移动、跨线程任务队列、置顶维护及钩子健康检查 |
| [NativeCursorWindow.cs](https://github.com/ziwuxin1/MAXsCursor/blob/748182434a23a0db275ed74b7d62bed3151daf36/src/MAXsCursor/Overlay/NativeCursorWindow.cs) | 小型透明分层窗口、32 位 DIB、圆环绘制、Alpha 预乘、上传与移动分离 |
| [HotkeyManager.cs](https://github.com/ziwuxin1/MAXsCursor/blob/748182434a23a0db275ed74b7d62bed3151daf36/src/MAXsCursor/Core/HotkeyManager.cs) | UI 线程 message-only 窗口、RegisterHotKey、MOD_NOREPEAT、快捷键回调分发 |
| [EventBus.cs](https://github.com/ziwuxin1/MAXsCursor/blob/748182434a23a0db275ed74b7d62bed3151daf36/src/MAXsCursor/Core/EventBus.cs) | 键盘和鼠标按钮事件队列；当前光圈移动路径已绕过队列 |
| [SettingsModel.cs](https://github.com/ziwuxin1/MAXsCursor/blob/748182434a23a0db275ed74b7d62bed3151daf36/src/MAXsCursor/Settings/SettingsModel.cs)、[SettingsStore.cs](https://github.com/ziwuxin1/MAXsCursor/blob/748182434a23a0db275ed74b7d62bed3151daf36/src/MAXsCursor/Settings/SettingsStore.cs) | 设置模型、版本迁移、快照复制；JSON 位于 AppData，临时文件写完后替换 |
| [SettingsWindow.xaml.cs](https://github.com/ziwuxin1/MAXsCursor/blob/748182434a23a0db275ed74b7d62bed3151daf36/src/MAXsCursor/Settings/SettingsWindow.xaml.cs) | 滑块实时预览、快捷键捕获、确定、取消及恢复默认 |
| [TrayIcon.cs](https://github.com/ziwuxin1/MAXsCursor/blob/748182434a23a0db275ed74b7d62bed3151daf36/src/MAXsCursor/Tray/TrayIcon.cs) | WinForms NotifyIcon、彩色/灰色状态、开关/设置/退出菜单 |
| [MAXsCursor.csproj](https://github.com/ziwuxin1/MAXsCursor/blob/748182434a23a0db275ed74b7d62bed3151daf36/src/MAXsCursor/MAXsCursor.csproj)、[DiagLog.cs](https://github.com/ziwuxin1/MAXsCursor/blob/748182434a23a0db275ed74b7d62bed3151daf36/src/MAXsCursor/Core/DiagLog.cs) | .NET 10、WPF + WinForms，明确允许原生库自解压；日志写入 TEMP |

其 `SPEC.md` 仍包含 .NET 8、全屏 WPF 叠加层和旧事件流，部分内容与实际代码不一致。本方案以代码为准。作者声明的 260 Hz 性能本次未运行验证，不作为本项目保证。

## 3. 参考项目实际结构与执行路径

```text
MAXsCursor/
  MAXsCursor.slnx
  src/MAXsCursor/
    Program.cs                     # 入口与单实例
    App.xaml / App.xaml.cs         # 生命周期和模块编排
    MAXsCursor.csproj
    app.manifest
    Core/
      HookManager.cs
      HotkeyManager.cs
      EventBus.cs
      KeyTranslator.cs             # 输入转为 HUD 文本
      DiagLog.cs
    Overlay/
      NativeCursorWindow.cs        # 原生光圈
      HudWindow.xaml(.cs)          # WPF HUD
      KeyboardHudLayer.cs
      RenderClock.cs               # HUD/波纹时钟
      ClickRippleController.cs
      ClickRippleWindow.cs
    Settings/                      # 模型、JSON、设置窗口、语言
    Tray/TrayIcon.cs
    Interop/                       # P/Invoke 与窗口常量
    Zoom/                          # 放大镜、屏幕捕获
    Assets/
  installer/                       # Inno Setup
```

启动时 Program 建立单实例，App 加载设置、创建 HUD 和 EventBus，再启动 HookManager 线程，并将初始光圈配置投递给它。光圈初始化、样式修改和显隐都在钩子线程执行；托盘、设置和全局快捷键由主线程处理。

鼠标移动路径：`WH_MOUSE_LL → MouseHookProc → FollowCursor → SetWindowPos`。不经过 WPF Dispatcher 或 EventBus。键盘和鼠标按钮事件才经队列进入 UI，服务 HUD 和波纹。

样式路径：`设置窗口 → 设置快照 → RunOnHookThread → ApplyRing → DrawRing → UpdateLayeredWindow`。普通移动复用已上传图像。RenderClock 不驱动静态光圈。

源码另有约 400 ms 维护定时器，恢复置顶并检查鼠标钩子疑似失效；前台窗口变化后短暂追加置顶操作，因此它仍存在低频定时工作。

## 4. 继承、裁剪和适配

| 参考设计 | 本项目决定 | 理由 |
| --- | --- | --- |
| Program + App | 保留等价职责 | 入口与模块生命周期清晰 |
| Core / Overlay / Settings / Tray / Interop | 保留目录分层 | 与参考源码逐层对应 |
| 专用钩子线程拥有光圈窗口 | 采用 | 设置界面繁忙时不影响光圈跟随 |
| 鼠标回调直接移动窗口 | 采用并测量耗时 | 缩短鼠标事件到窗口更新的路径 |
| 键盘低级钩子、KeyTranslator、EventBus | 移除 | 本项目改用键盘 Raw Input，不占用全局快捷键，也无需输入 HUD |
| HUD、波纹、Zoom、RenderClock | 移除 | 不属于鼠标位置提示需求 |
| WPF / WinForms | 改为 Win32 窗口与托盘 | 避免携带并解压 .NET/WPF 原生组件 |
| JSON 设置和 TEMP 日志 | 便携 JSON 和内存错误状态 | config/settings.json 保存设置，不生成日志 |
| 自启动 | 新增 StartupManager | 核查的应用代码未发现对应实现 |
| 固定光圈缓冲区、启动时单次 DPI 读取 | 动态尺寸、逐屏 DPI 处理 | 避免大圆环裁切，适配混合缩放 |
| 仅圆环绘制 | 新增实心高亮/空心光圈互斥选择 | 两种模式均保留原有系统光标图标 |
| 单文件发布和安装器 | 单 EXE 程序与便携配置/素材 | 用户确认允许 config 与 styles，不引入安装器 |

这是按职责和算法转写，不将参考仓库作为运行依赖。如复制或实质改写其源码，按 MIT 许可保留版权和许可全文，嵌入 EXE 的“关于/开源许可”界面；不要求用户携带独立许可文件。[参考许可证](https://github.com/ziwuxin1/MAXsCursor/blob/748182434a23a0db275ed74b7d62bed3151daf36/LICENSE)

## 5. 本项目建议结构

以下为已编写的源码布局，文件命名按 Google 风格使用小写及下划线：

```text
mouse_cursor_detector/
  ARCHITECTURE.md / ENVIRONMENT_CHECKLIST.md / README.md
  CMakeLists.txt / CMakePresets.json
  .clang-format / .vscode/
  .reference/MAXsCursor/              # 分析材料，不参与编译发布
  src/mouse_cursor_detector/
    program.cc                       # wWinMain、单实例、已有实例通知
    app.h / app.cc                   # 主线程编排、状态、错误提示
    core/
      hook_manager.h / .cc           # 钩子线程、命令、健康检查
      hotkey_manager.h / .cc         # 全局快捷键及绑定替换
      startup_manager.h / .cc        # 登录自启动
    overlay/native_cursor_window.h / .cc
    settings/
      settings_model.h
      settings_store.h / .cc         # 旧版注册表迁移
      settings_window.h / .cc        # 原生设置、预览、取消
    tray/tray_icon.h / .cc
    interop/window_messages.h
    resources/
      app.rc / app.manifest / third_party_notices.txt
  scripts/build.ps1                  # 功能确认后手动执行
  build/                             # 后续构建中间文件
  dist/MouseCursorDetector.exe        # 后续唯一交付物
```

关键接口为 `HookManager::Start/Stop/ApplyState`、`NativeCursorWindow::ApplyStyle/FollowCursor/SetVisible/ReassertTopmost`、`HotkeyManager::Replace/Reset`、`LoadSettings/SaveSettings`、`QueryStartup/SetStartupEnabled`。显隐和样式统一通过快照投递，模块全部链接进一个 EXE。使用已确认的多尺寸自定义 ICO；图标、许可和清单内嵌，不依赖旁置图标文件。

## 6. 线程与生命周期

采用单进程。主线程管理隐藏顶层控制窗口、托盘、快捷键、设置和配置文件；导入时启用独立异步任务进行素材复制和解码。钩子线程建立自己的命令窗口，拥有鼠标钩子、光圈、前台事件和维护定时器。

```text
主线程                                  钩子/光圈线程
托盘 / WM_INPUT / 设置
  └─ 投递显隐、最新样式快照 ──────────→ 命令窗口
                                         ├─ ApplyStyle
                                         └─ SetVisible
                                         WH_MOUSE_LL
                                           └─ FollowCursor → SetWindowPos
  ←───────── 初始化结果、状态和错误 ─────┘
```

启动顺序：单实例 → 加载校验设置 → 控制窗口和托盘 → 注册快捷键 → 钩子线程初始化 → 明确报告初始化成功 → 保持提示隐藏，仅在用户开启时显示。失败保留可操作的托盘及设置，不能将超时视为成功。

钩子线程先建立消息队列和命令 HWND，再报告可接受命令。主线程使用 PostMessage 唤醒；跨线程内容用受短锁保护的固定设置快照，连续样式更新合并，只保留一个待处理通知。鼠标回调不取得该锁，不同步等待 UI，不跨线程 SendMessage。

鼠标回调只记录位置和时间、移动已有窗口、调用 CallNextHookEx；不分配内存、不绘图、不读写文件/注册表、不重建 DPI 资源。线程首版使用正常优先级，根据测量决定是否调整。

退出顺序：停止收取新命令 → 注销快捷键 → 通知钩子线程退出 → 线程内移除钩子、事件和定时器，销毁光圈及 DIB → 等待实际线程结束 → 清理托盘和控制窗口。共享资源在线程退出后释放，避免仅等待固定超时就提前销毁。

## 7. 双模式提示绘制与原光标保护

使用 WS_POPUP 窗口，组合 WS_EX_LAYERED、WS_EX_TRANSPARENT、WS_EX_NOACTIVATE、WS_EX_TOPMOST、WS_EX_TOOLWINDOW。提示层不抢焦点、不出现在任务栏、不阻挡下方点击。两种模式均不截获 WM_SETCURSOR 来设置底层应用的指针，不绘制替代指针，不读取或重写原有光标图像。

沿用参考项目“图像缓存、移动复用”的方式：

1. 依据半径、线宽和 DPI 分配自顶向下的 32 位 BGRA DIB，保留抗锯齿边距。
2. 使用 Windows GDI+ 在明确像素格式的透明图像上绘制：Highlight 模式填充半透明圆盘，Ring 模式只绘制圆环。直接使用 PixelFormat32bppPARGB 缓冲区，由 GDI+ 完成预乘，避免重复预乘。
3. 调用 UpdateLayeredWindow 上传。只在模式、颜色、半径、线宽、透明度或 DPI 变化时重绘上传；切换模式先清空缓冲区，避免残留另一种效果。
4. 鼠标移动只调用 SetWindowPos，使用 SWP_NOSIZE、SWP_NOACTIVATE、SWP_NOZORDER；不重绘、不上传图像。
5. 提示中心对齐鼠标热点；Ring 模式中心 Alpha 为 0，Highlight 模式中心使用配置的低透明度底色。原有系统指针保持独立显示，验证两种模式都不会遮住指针细节。

缓冲区边长按 `2 × (半径 + 线宽 / 2 + 边距) × DPI / 96` 向上取整。先校验参数，再分配；RAII 管理 HWND、DC、DIB 和 GDI+ 生命周期，释放位图前恢复 DC 的原选入对象。

光圈默认值参考上游：绿色 #02FF60、半径 36 DIP、线宽 7 DIP、透明度 90%。高亮默认值为黄色 #FFFF00、半径 24 DIP、透明度 25%。两种模式分别保存外观参数，切换不互相覆盖；显示期间均为静态外观；主线程计时默认 3 秒后隐藏，可设置 1–3600 秒，没有常驻动画时钟。再次触发重设计时器及绝对截止时间，长按不重复触发；以绝对截止时间校验过期消息，避免旧计时消息关闭新一轮提示。时长修改从下次触发生效。

高亮模式透明度限制为 10%–50%，用于保持底层内容和原指针可辨认；光圈模式为 10%–100%。原指针形状随应用自然变化，提示层始终围绕热点跟随，不将任何形状强制变为箭头。暂停或退出只销毁/隐藏本程序的窗口，不执行系统光标恢复操作。

## 8. 交互与设置

| 操作 | 行为 |
| --- | --- |
| 双击 EXE（首次或后续手动启动） | 提示效果关闭、进入托盘、打开设置 |
| 登录自启动 | 提示效果关闭、进入托盘，不打开设置；等待快捷键触发提示 |
| 逗号键 + 句号键（`,` + `.`） | 默认同时按下触发，顺序不限；到时自动关闭，再次触发重新计时；可改为单键或组合键，按住仅触发一次 |
| 双击托盘 | 切换高亮 |
| 设置中的“提示方式”单选项 | “高亮光标（半透明底色）”/“光圈提示（空心圆环）”，仅选其一 |
| 切换提示方式 | 开启时立即预览新模式；关闭时保持关闭，仅更新所选方式 |
| 托盘右键 | 启用/禁用、设置、登录自启动、关于、退出 |
| 修改样式 | 实时预览；确定保存；取消恢复打开前样式 |
| 重复启动 EXE | 通知已有实例打开设置，新进程退出 |
| 打开设置 | 具有 WS_EX_APPWINDOW 的设置窗口显示任务栏按钮，托盘继续驻留 |
| 最小化设置 | 系统原生最小化，保留任务栏按钮、托盘及草稿 |
| 关闭设置 | SW_HIDE 隐藏设置并移除任务栏按钮，保留页面、草稿及托盘 |
| 取消主设置 | 恢复最近保存的设置和语言，销毁设置窗口；进程驻留托盘 |
| Language/语言 | 即时切换全部应用文案，随“保存设置”提交；不重建输入控件 |

SettingsModel 保存 hotkey_modifiers、hotkey_vk、hotkey_secondary_vk（0 表示单键）、hold_seconds，并包含 `IndicatorMode = Highlight | Ring`。外观分为 `HighlightStyle { Color, RadiusDip, Opacity }` 和 `RingStyle { Color, RadiusDip, ThicknessDip, Opacity }`。半径 8–100 DIP、环线宽 1–20 DIP 且小于直径；透明度按模式范围校验。未知模式回退 Ring。旧版注册表存储版本为 3，共 15 个 DWORD；迁移时兼容版本 1、2 的 14 个 DWORD 记录。索引 13 保存时长，索引 14 保存第二个普通键；版本 1 缺失时长使用 3 秒，旧版缺失第二键使用 0，保留已有单键绑定。未知版本回退默认值并提示。

设置窗口只显示当前模式可用的外观控件：实心高亮没有线宽选项，光圈提供线宽。快捷键触发整个提示层并重新计时；托盘可触发提示或提前关闭，不负责切换模式。设置预览涵盖模式及两套样式，确定后持久化，取消同时恢复旧模式和样式。托盘提示文本显示当前方式及启用状态。

主线程保存已提交设置和预览设置两份值对象。光圈线程只接收快照。取消恢复已提交样式；快捷键通过 RIDEV_INPUTSINK 接收键盘 Raw Input，不调用 RegisterHotKey，不检测其他应用或系统组合键占用；按键照常传递。记录左右修饰键状态并按按下沿匹配，避免长按重复触发。确定时应用新绑定；保存失败恢复原绑定。支持单键、两个普通键以及附加 Ctrl、Alt、Shift、Win 的组合；默认逗号加句号，任一顺序按齐均触发。点击输入框后直接按键捕获；新默认值不覆盖已保存的绑定。软件启动后立即监听键盘，提示隐藏或到时关闭不会停止监听。设置窗口打开期间仍可触发并显示当前预览外观，仅快捷键录入框拥有焦点时暂不触发，避免录入操作触发提示；时长使用已保存值。

自启动是独立、立即生效的系统开关：写入成功才更新勾选，并标注“立即生效”，不混入样式预览取消语义。设置确定保存失败时保持窗口打开，并尝试恢复原快捷键。提示开关只影响本次运行，不写入持久化设置；每次启动均为关闭。旧设置中记录的开启状态会被忽略；旧 REG_BINARY 只读迁移，显示状态不写入 JSON。

## 9. 自启动和无旁置文件的持久化

当前配置使用 EXE 旁的 config/settings.json（平面 JSON，版本 2，兼容读取版本 1），通过临时文件刷新后替换。没有 JSON 时读取旧 HKCU\Software\MouseCursorDetector 的 REG_BINARY 版本 1–3 记录并迁移；不再回写旧记录。素材的 styles/ 相对路径、模式、参数和快捷键随 JSON 保存。样式库以 style_count、style_N_name、style_N_file 记录名称和独立路径；导入验证后弹窗命名，留空使用原文件名，严格区分大小写拒绝完全同名。二级窗口保存成功后名称才加入列表并持久化，重启后可继续选择。磁盘文件使用唯一名称，避免显示名称大小写差异造成文件冲突。加载失败回退并提示，不自动覆盖损坏配置。详见 docs/PORTABLE_STYLES.md。

2026-11-05 添加可选 `ui_language` 无符号字段（0 中文、1 英文），缺失、类型不符或越界时使用中文；JSON 版本及旧注册表记录布局保持不变。`localization.h/.cc` 内嵌中英文文案，UI 在显示边界调用 `Tr()`；素材后台任务和配置加载继续返回原有错误文本，主线程显示时才翻译，用户名称、路径和许可原文不参与翻译。语言草稿即时预览到界面和托盘；取消恢复已提交语言，单独保存素材库不提交语言。系统对话框及系统错误文本由 Windows 决定。

StartupManager 管理 HKCU\Software\Microsoft\Windows\CurrentVersion\Run 的 MouseCursorDetector 值，写入带引号的当前 EXE 绝对路径，例如：

```text
"C:\Tools\MouseCursorDetector.exe" --autostart
```

设置窗口提供可勾选的“开机自启动”选项，默认关闭，用户启用后仅在当前用户登录时启动软件并进入托盘，提示效果保持关闭，无需管理员权限，系统可能延迟执行。界面读取实际 Run 路径；该值存在不能保证系统“启动应用”未被用户禁用。

EXE 不复制自身。移动或改名后，用户需从新位置运行；检测到本工具的旧启动路径时提供修正提示。关闭自启动仅删除本程序值；删除程序前可关闭自启动；如需清除已保存的样式，可手动删除本程序设置键。

默认错误保留于内存并在托盘/设置显示；当前通过错误提示报告故障，不写入 TEMP 文件日志。系统自身的缓存与诊断记录不属于本程序主动释放的文件。

## 10. DPI、置顶与故障恢复

嵌入清单声明 Per-Monitor V2，窗口移动与鼠标位置统一使用屏幕物理坐标，支持负坐标、多屏和竖屏。样式采用 DIP，跨屏时换算到目标 DPI。

鼠标回调仅投递合并后的显示器检查通知；DPI 查询和重绘在消息处理器执行。缓存当前 DPI，在合并位置检查及低频维护中查询目标显示器 DPI，变化后重新计算尺寸，避免照搬启动时单次读取 DPI 的方式。重建前可短暂复用旧图像，随后在当前位置替换。

启用时保留约 400 ms 低频维护：检查实际鼠标位置和最后钩子事件，疑似失效时限频重装并同步位置。前台变化后恢复置顶，低频维护再次确认置顶。禁用时隐藏光圈、卸载鼠标钩子、停止维护和前台监听；重新启用读取当前位置再显示。

钩子健康判断属于启发式，软件移动鼠标也可能产生位置差异。重装须检查返回值、限频，连续失败提示用户，避免无限重试；UI 按实际完成状态更新。

低级鼠标钩子超时可能被 Windows 静默移除，必须测量回调耗时，不能将“同线程 SetWindowPos”视作绝不阻塞。若压力验证发现耗时异常，回调改为仅记录最新位置并投递合并消息，再由同线程窗口过程执行移动。[官方说明](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc)

普通桌面、窗口化和无边框应用列为支持范围；独占全屏、锁屏、UAC 安全桌面不保证覆盖。控制窗口接收 Explorer 重启广播恢复托盘；锁定时隐藏，解锁后按原状态恢复。

## 11. 构建与发布

技术栈为 C++17、MSYS2 UCRT64 MinGW-w64 GCC、CMake、Ninja；Windows 头文件和导入库由 MinGW-w64 提供。链接使用 -static、-static-libgcc、-static-libstdc++、Unicode 入口、GUI 子系统和 asInvoker 权限。GDI+、User32、Gdi32、Shell32、Advapi32 等使用系统组件，不引入第三方运行时。

设置界面代码、清单、默认值和需要保留的许可全文均编入 EXE，托盘使用系统共享图标。构建只显式包含本项目源码，排除 .reference。不制作安装器、不使用自解压打包器、不在运行时下载内容。

build.ps1 完成配置、Release 构建、依赖检查和发布。dist 只放 MouseCursorDetector.exe；调试信息和中间文件保留于 build。使用 objdump -f/-p 检查 x64 PE 及直接 DLL 导入，拒绝 GCC 动态运行库与非系统 DLL，再在无开发环境的 Windows 上验证。

## 12. 实施和验收

实施顺序：入口/托盘 → 静态原生光圈 → 专用钩子线程 → 快捷键和设置 → 自启动/DPI/恢复 → 单文件交付验证。

| 验收项 | 合格标准 |
| --- | --- |
| 范围 | 只有光标高亮和必要设置，不出现居中、HUD、放大镜、波纹入口 |
| 开关 | 快捷键与托盘状态一致，默认逗号加句号任意顺序触发，单独一个键不触发；按住不重复触发，默认 3 秒后隐藏，修改时长有效，再次触发重新计时；每次启动默认关闭；即使上次开启或旧记录为开启，也不自动显示提示 |
| 双模式 | 高亮为半透明实心底色，光圈为空心圆环；切换无叠加残影，独立样式可恢复 |
| 绘制与输入 | 光圈中心透明；高亮底色不遮住原指针细节；两种模式均无黑底黑边、点击穿透、不改变前台焦点 |
| 原光标保护 | 启动、两种模式切换、暂停、正常退出和强制终止后，系统光标方案、文件及注册表不变；箭头、文本、手形、缩放、忙碌及自定义指针照常切换且不被隐藏 |
| 移动成本 | 常规移动不重绘、不调用 UpdateLayeredWindow |
| 线程隔离 | 设置操作期间持续跟随，回调不执行磁盘/注册表 I/O |
| 设置 | 取消恢复模式和两套样式；关闭状态切模式不显示提示；重启恢复模式；单键和组合键可保存、重叠快捷键不阻止保存；未知版本及未知模式回退有效 |
| 多屏 DPI | 验证 100%/150%/200% 混合缩放、负坐标、竖屏和大半径无裁切 |
| 生命周期 | 重复启停、锁定解锁、睡眠唤醒和退出无悬挂线程或资源持续增长 |
| 恢复 | 模拟钩子安装失败/失效可提示并限频恢复，Explorer 重启可恢复托盘 |
| 自启动 | 启用后重新登录可运行，关闭后不启动，中文/空格路径有效 |
| 单实例 | 重复启动仅一套托盘、钩子和光圈 |
| 发布与便携 | 干净系统可运行 EXE，默认创建 config；复制整个软件文件夹可保留用户素材；不释放 DLL、脚本、辅助 EXE 或日志 |
| 性能 | 记录目标机器不同刷新率、高轮询鼠标下的回调耗时和 CPU，不沿用未经复测的上游性能保证 |
| 禁用占用 | 无鼠标钩子、无维护定时器；保留键盘 Raw Input 以便快捷键再次开启 |

目前仅完成首版源码、构建配置、静态审查和格式检查。上述程序行为均仍须后续编译及运行验收；已有首轮构建产物但尚未运行，不将构建或静态检查视为功能通过；后续编译只由用户执行。






