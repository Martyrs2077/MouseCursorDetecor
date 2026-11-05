# 自定义光圈样式与 MP4 可行性评估

评估日期：2026-11-02。本文保留初始技术评估与方案比较。后续用户已确认 EXE 旁 config/styles 便携目录；静态图片、GIF 与便携配置已接入源码，当前行为与限制以 PORTABLE_STYLES.md 为准。MP4 仍仅评估，未实现。所有新增功能均未由助手配置构建、编译或运行验证。

## 结论

可以增加样式选项，保留“半透明高亮”和“空心光圈”，增加“静态图片”和“GIF 动画”。不要将“各类图片格式”解释为所有图片格式无条件兼容：应公布稳定支持清单、可选解码器支持清单及不支持时的提示。

MP4 技术上可行，但应作为独立后续功能。首个候选范围为系统能够解码的 H.264 MP4、静音、限尺寸、触发期间循环。不能承诺任意 MP4 或天然透明的视频特效。该结论来自源码与系统接口文档检查，不代表播放性能或兼容性已经通过实测。

原有约束继续有效：软件启动立即监听键盘；默认逗号加句号触发；按配置时长显示、到时隐藏并继续监听；不替换或隐藏系统鼠标光标；单 EXE 发布；编译由用户执行。

## 格式范围建议

| 格式 | 建议支持方式 | 需要说明的行为 |
| --- | --- | --- |
| PNG | WIC 解码，正式支持 | 优先用于带透明背景的静态特效，保留 Alpha |
| JPEG/JPG、BMP | WIC 解码，正式支持 | 没有透明信息的图片按矩形显示，不能自动变成空心光圈 |
| TIFF/TIF | WIC 解码，正式支持常规文件 | 多页文件首版只取第一页，避免把多页误当动画 |
| ICO | WIC 解码，正式支持 | 按目标尺寸选择合适图层，不把各尺寸当动画帧 |
| GIF | WIC 解码与帧合成，正式支持动画 | 帧延时、偏移、局部帧、透明色及 disposal 必须正确处理，不能只显示第一帧 |
| JPEG XR/WDP/HDP | 可通过 WIC 扩展支持 | 不必放入首版常用格式筛选器 |
| WebP、HEIC/HEIF、AVIF 等 | 依机器实际安装的解码器探测；或另行集成解码器 | 不作为纯系统接口方案的跨机器保证；动画能力还需逐格式适配 |
| APNG、动态 WebP、SVG、PSD、RAW 等 | 首版不承诺完整支持 | 静态解码、动画合成、矢量渲染和分层工程文件是不同功能，不能仅增加扩展名就宣称支持 |

依据：WIC 系统编解码器表列出了 BMP、GIF、ICO、JPEG、JPEG XR、PNG、TIFF 等，并支持发现额外安装的解码器。[1] 微软 GIF 示例专门处理帧偏移、帧延时、disposal 和合成，说明逐帧读取本身不足以正确播放。[2]

如果必须在不安装任何扩展的 Windows 上稳定支持 WebP/AVIF 等，需要为明确的目标格式静态集成对应解码库，再检查 GCC 兼容性、许可和 EXE 体积。当前不下载依赖，也不声称已支持这些格式。

## 软件内的选项建议

- 提示样式：半透明高亮 / 空心光圈 / 自定义图片 / GIF 动画。
- 素材：选择文件、预览、清除；导入失败时保留之前有效素材及设置。
- 外观：显示尺寸、保持宽高比、整体不透明度、中心偏移。图片模式隐藏无意义的线宽与颜色控件。
- 透明度：保留素材自身透明像素，整体不透明度再乘以原有 Alpha。JPEG 等不自动去底色；如需去底色，应另加明确的颜色键选项。
- GIF：建议每次有效触发从第一帧开始，按帧时长循环，到提示总时长截止立即隐藏；再次触发重置动画与倒计时。文件自身有限循环次数是否保留，应在实施时确定并写入说明。
- 设置窗口打开时可预览；在快捷键捕获框输入新键时避免误触发。

## 对现有结构的修改方案

当前 NativeCursorWindow::ApplyStyle 使用 GDI+ 在 PARGB DIB 上绘制圆形，再通过 UpdateLayeredWindow 上传；FollowCursor 仅改变位置。HookManager 的鼠标钩子与该窗口共用线程，当前 400 ms 定时器只做健康维护，不能充当 GIF 帧调度器。

建议保留现有窗口及移动路径，新增媒体加载模块和不可变的帧数据对象：

1. 新增媒体解码模块，使用 WIC 验证文件内容、读尺寸、处理旋转方向及色彩、选择图层或第一页，输出统一的预乘 BGRA 像素。GIF 先正确合成完整画布帧，再用于播放。
2. 导入、解码和缩放放到独立加载线程，不能放入低级鼠标回调，也不能长时间阻塞拥有鼠标钩子的消息线程。设置窗口异步显示导入结果；取消或快速更换文件时丢弃过期结果。
3. 帧数据使用受控生命周期的只读对象共享；禁止将未经约束的大量像素复制进每次鼠标事件或每份 Settings 快照。
4. NativeCursorWindow 增加位图帧上传入口，记录实际宽高和锚点；移动仍只更新位置。窗口表面复用，避免每帧重复创建 DC、DIB。静态素材不启用动画定时器。
5. GIF 使用独立的帧调度及单调时钟；延迟时允许跳过已过期帧，不能高速补绘所有旧帧。只在当前提示可见期间调度，到时隐藏、锁屏或退出立即停止。主线程总时长控制优先于媒体播放进度。
6. 设置存储迁移到下一版本，保留旧模式、时长、双键绑定和两套样式；自定义素材使用独立受限的二进制值，不能直接扩写现有固定 15 DWORD 记录并让旧读取逻辑误读。
7. 资源读取失败、文件过大或格式不支持时给出具体原因；不得因此关闭键盘监听。跨屏 DPI 只重新选择或缩放当前样式，不无条件重置 GIF 播放进度。

资源限制需要在实现时明确：文件大小、源图像素、总帧数、解码后缓存字节数、播放帧率和显示尺寸均应有上限。小文件也可能解码成大量像素，必须在分配前使用溢出检查。不能把无限制预解码作为默认方案。

## 单 EXE 与自定义素材持久化

必须区分“软件发布只有一个 EXE”与“用户选择的素材也物理存放在这个 EXE 内”。运行时导入的任意文件不会自动出现在已经构建好的资源段中。

| 方式 | 是否只发布一个 EXE | 删除导入源后可用 | 换电脑只带 EXE 保留自定义素材 | 取舍 |
| --- | --- | --- | --- | --- |
| 构建时将素材作为 RCDATA 内嵌 | 是 | 是 | 是 | 更换内置素材后需要用户重新构建 |
| 运行时导入至内存，不持久化 | 是 | 当前运行可用 | 否 | 重启需重新导入 |
| 运行时导入并将受限素材保存至 HKCU | 是，无旁置文件 | 是 | 否 | 使用机器本地配置，须限制素材体积；不是素材物理内嵌 EXE |
| 只保存原文件路径 | 软件 EXE 是单个，效果依赖外部素材 | 否 | 否 | 不符合所有效果独立于外部文件的严格要求 |

严格维持“带走一个 EXE 就包含全部已选素材”时，采用构建时内嵌方案。若允许沿用现有 HKCU 配置方式保存用户自定义素材，则可增加运行时导入，不要求运行时附带图片文件，但不能宣称素材随 EXE 自动跨机器迁移。

WIC 可从内存流读取图像，因此内嵌或注册表素材不必先释放成临时文件；必须保证流和底层字节的生命周期符合接口要求。[3] 不建议为此实现运行中改写自身 EXE。

## MP4 独立可行性检查

### 接口与依赖

建议使用 Windows Media Foundation 的 Source Reader 解封装并解码，不启动外部播放器，不引入动态 FFmpeg DLL。MP4 是容器，文件源只负责解析，实际视频编码仍必须有可用解码器；不能根据 .mp4 扩展名判断必然能播放。[4]

Source Reader 可以提供解码后帧与时间戳，也可从 IMFByteStream 构建；由应用负责排程和显示，因而内嵌视频可以规划为内存流读取，不要求落地临时 MP4。[5][6] 候选链路为：内存字节流 → MP4 文件源 → H.264 解码 → 缩放/颜色转换 → 预乘 BGRA → 分层窗口。需单独处理视频旋转、像素宽高比、stride、帧时间戳、seek、循环和取消。

本机静态检查结果：C:\GCC\ucrt64 下存在 wincodec.h、mfapi.h、mfidl.h、mfreadwrite.h，以及 windowscodecs、mfplat、mfreadwrite、mfuuid 导入库。System32 下存在 windowscodecs.dll、mfplat.dll、mfreadwrite.dll、msmpeg2vdec.dll。只能证明开发接口文件和系统组件文件存在，不能证明指定素材已成功解码、硬件加速可用或在其他 Windows 版本同样可用。

### 透明背景的实际限制

微软 H.264 解码器公布的输出为 I420/IYUV/NV12/YUY2/YV12，并不提供独立 Alpha 通道。[7] RGB32 与 ARGB32 是不同的视频输出类型，前者并不意味着自带透明信息。[8]

据此推断：普通 H.264 MP4 适合带背景的小视频贴片；要得到空心光圈或透明粒子，需要应用另外生成 Alpha，例如圆形裁剪、固定中心挖空或色键去底。给整幅画面加不透明度只能半透明，不能自动消除黑色矩形。带 Alpha 的特殊视频编码或双轨布局需要另设格式约定，首版不应承诺自动识别。

### 性能与工程代价

视频解码和颜色转换应在独立线程/异步回调内完成，使用有限帧队列，鼠标钩子绝不等待解码。微软指出 Source Reader 的部分软件视频转换不推荐直接用于实时视频渲染，因此不能未经测试就承诺该链路低开销；高分辨率或较高帧率时需评估 D3D11/视频处理路线，改动会明显大于 GIF。[5]

建议试验从小尺寸、短时长、SDR、H.264、无声素材开始，视频只在提示可见时播放。总时长仍由用户的 3 秒或自定义值控制。HEVC/AV1、HDR、特殊透明视频不纳入第一轮保证。

### 尚未执行的验证与放行条件

由于用户禁止助手编译，本次没有制作或执行播放原型，也未修改现有功能代码。后续可编写由用户手动构建的试验入口，验证：

- 静态图片透明边缘、非方形缩放、方向元数据、多页 TIFF、ICO 最佳尺寸选择及损坏文件。
- GIF 局部帧、透明背景、disposal 2/3、不同帧延时、零延时、重触发和隐藏后的资源释放。
- MP4 实际编码探测、首帧延迟、静音、循环、总时长中断、重触发、彩色边缘与颜色键效果。
- 鼠标移动时的延迟、CPU、峰值内存、钩子超时和跨 DPI 状态；具体性能数据以目标机器测量为准。
- 只有 EXE 的干净 Windows 10/11 环境；媒体组件缺失时友好拒绝并保留普通光圈功能。

建议交付顺序：静态图片 → GIF → 独立 MP4 播放试验 → 再决定是否纳入正式样式选项。

## 官方依据

1. WIC Overview / Native Codecs：https://learn.microsoft.com/en-us/windows/win32/wic/-wic-about-windows-imaging-codec
2. Microsoft WIC Animated GIF Sample：https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/multimedia/wic/wicanimatedgif/WicAnimatedGif.cpp
3. IWICStream::InitializeFromMemory：https://learn.microsoft.com/en-us/windows/win32/api/wincodec/nf-wincodec-iwicstream-initializefrommemory
4. MPEG-4 File Source：https://learn.microsoft.com/en-us/windows/win32/medfound/mpeg-4-file-source
5. Source Reader：https://learn.microsoft.com/en-us/windows/win32/medfound/source-reader
6. MFCreateSourceReaderFromByteStream：https://learn.microsoft.com/en-us/windows/win32/api/mfreadwrite/nf-mfreadwrite-mfcreatesourcereaderfrombytestream
7. H.264 Video Decoder：https://learn.microsoft.com/en-us/windows/win32/medfound/h-264-video-decoder
8. Video Subtype GUIDs：https://learn.microsoft.com/en-us/windows/win32/medfound/video-subtype-guids
