# GCC / VS Code 环境安装与检查记录

检查日期：2026-10-31（Asia/Hong_Kong）。

用户已授权助手下载并安装至 `C:\GCC`，覆盖此前禁止代为下载的要求。当前采用 **VS Code + MSYS2 UCRT64 GCC + GDB + CMake + Ninja**。用户已确认功能，并要求后续编译仅由用户执行。环境安装阶段未编译项目；之后助手发起的一次构建在中断前已生成 EXE，目前无遗留构建进程，程序尚未启动或进行功能验收。

## 1. 安装结果

- MSYS2 安装根目录：`C:\GCC`；安装前该目录为空，安装程序退出码为 0。
- 安装器：`msys2-x86_64-20260927.exe`，从清华大学 TUNA 镜像下载。
- 下载文件 SHA-256 与 MSYS2 官方 GitHub 发布的校验值一致：

  ```text
  416d506c839a2da4bc184eeec8cbaaf1055ee698618228c62dfc4232a6970f0e
  ```

- Windows Authenticode 验证状态为 Valid，签名者为 Christoph Reiter。
- 软件包首选镜像：清华 TUNA；原 mirrorlist 保存在 `C:\GCC\etc\pacman.d\mirrorlist.*.before-tuna.bak`。
- 已完成核心及完整系统更新，通过 pacman 安装 UCRT64 toolchain、CMake、Ninja，共 66 个软件包（包含依赖）。
- 安装器、安装日志及版本检查 JSON 位于 `%TEMP%\mouse_cursor_detector_toolchain`。

参考入口：[MSYS2 安装说明](https://www.msys2.org/docs/installer/)、[清华镜像目录](https://mirrors.tuna.tsinghua.edu.cn/msys2/distrib/x86_64/)、[官方校验值](https://github.com/msys2/msys2-installer/releases/download/2026-09-27/msys2-x86_64-20260927.exe.sha256)。

## 2. 逐项检查证据

| 项目 | 实际结果 | 状态 |
| --- | --- | --- |
| Windows | Windows 11 专业版 x64，10.0.26200 | 系统信息已确认 |
| VS Code | 1.142.0 x64 | 已安装，CLI 可运行 |
| C/C++ 扩展 | ms-vscode.cpptools 1.34.4 | 已安装；编辑器解析待交互确认 |
| CMake Tools 扩展 | ms-vscode.cmake-tools 1.24.42 | 已安装；配置构建待执行 |
| Clang-Format 扩展 | xaver.clang-format 1.9.0 | 已安装 |
| clang-format 程序 | C/C++ 扩展内置 LLVM，23.1.0 | 19 个源码文件 Google 格式检查已通过 |
| GCC / g++ | 16.2.0，MSYS2 Rev6 | 版本命令退出码为 0 |
| 编译目标 | x86_64-w64-mingw32 | -dumpmachine 检查通过 |
| GDB | 18.1 | 版本查询与 batch show configuration 初始化通过 |
| CMake | 4.4.5 | 版本命令退出码为 0 |
| Ninja | 1.13.2 | 版本命令退出码为 0 |
| windres / objdump | Binutils 2.47.20260726 | 版本命令退出码为 0 |
| UCRT64 shell | MSYSTEM=UCRT64；g++=/ucrt64/bin/g++ | 登录 shell 查询通过 |
| 软件包完整性 | GCC 1163、GDB 199、CMake 4260、Ninja 7 个文件，均 0 altered | pacman -Qkk 检查通过 |
| Git | 2.55.0.windows.5 | 已安装，版本命令成功 |
| 单 EXE 发布 | 静态链接与内嵌资源已配置 | 已有首轮构建产物，未运行验收 |

本次工具执行路径均位于 `C:\GCC\ucrt64\bin`。安装成功与工具能启动不等于项目已通过编译、调试或功能验收。

## 3. VS Code 配置

已更新项目的 `CMakePresets.json`、`.vscode/settings.json`、`.vscode/launch.json`、`scripts/build.ps1`，统一指向 `C:\GCC\ucrt64\bin`。

- C/C++ compilerPath 指向 `g++.exe`，语言标准 C++17，IntelliSense 模式 windows-gcc-x64。
- CMake 指向本工具链的 `cmake.exe`，preset 使用 Ninja；打开项目时不自动配置。
- GDB 指向本工具链的 `gdb.exe`；调试配置不附带自动构建任务。
- 已添加项目终端配置 **MSYS2 UCRT64**，从 VS Code 终端面板的新建终端菜单选择即可。
- 未修改全局用户或系统 PATH；UCRT64 终端和构建 preset 为相应进程配置 PATH。
- 相关 JSON 均已解析验证，构建脚本语法已解析，脚本未执行。

如 VS Code 已打开项目，可重新加载窗口后新建上述终端，并执行：

```bash
g++ --version
gdb --version
cmake --version
ninja --version
```

## 4. 功能确认之后的联合验收

- [ ] 在 VS Code 手动选择 gcc-debug，配置与构建成功，确认编译器为 UCRT64 g++。
- [ ] GDB 启动程序、命中断点、查看变量并正常退出。
- [ ] Release 构建成功，windres 嵌入清单及许可，GUI 子系统正确。
- [ ] objdump 检查 x64 与 DLL 导入，不依赖 GCC 或 MSYS 动态运行库 DLL。
- [ ] 空目录及无开发工具的 Windows 上可运行 EXE 并创建便携配置；复制整个软件目录可恢复用户样式。
- [ ] 按 README 完成两种提示模式、原光标保护、自启动、设置、多屏及生命周期验收。

当前结论：**开发工具安装及无需编译的检查通过；已有首轮构建产物；后续构建由用户手动执行，调试、功能和单文件运行验收尚未执行。**

