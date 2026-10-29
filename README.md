# 青影 QingYing

[English](#english) · 简体中文

简洁、好看的 Windows 桌面截图工具。靛蓝主色、悬浮圆角工具条，只在需要时出现二级选项。

> 当前版本：0.7.0（RapidOCR 本地识别、OpenAI 兼容翻译）。下方下载仍是已发布的 0.6.6；0.7.0 安装包在打上 `v0.7.0` 标签后出现在 Releases。
>
> **国内下载（推荐，免翻墙）**
> - 安装包直链：[QingYing-0.6.6-windows-x64-setup.exe](https://gitee.com/liner123/qing-ying/releases/download/v0.6.6/QingYing-0.6.6-windows-x64-setup.exe)
> - 免安装 ZIP：[QingYing-0.6.6-windows-x64-portable.zip](https://gitee.com/liner123/qing-ying/releases/download/v0.6.6/QingYing-0.6.6-windows-x64-portable.zip)
> - 全部版本：[Gitee Releases](https://gitee.com/liner123/qing-ying/releases)
>
> GitHub 镜像同步；应用内「检查更新」仍使用 GitHub Releases。

## 功能

- **托盘常驻**：单击托盘图标截图；右键菜单包含截图、设置、开机自启、关于、退出
- **全局热键**：截图默认 `Ctrl+Alt+A`，录屏默认 `Ctrl+Alt+R`，识别并翻译默认 `Ctrl+Alt+T`，均可在设置中修改，被占用或彼此冲突时会提示
- **区域截图**
  - 冻结所有显示器，每块屏幕按各自 DPI 像素精确截取
  - 自动识别光标下的窗口和控件，单击即可选中；`Tab` 切换"窗口 / 窗口+控件"识别
  - 拖拽框选，8 个手柄调整大小，拖动选区内部移动
  - 圆形放大镜（13×13 像素网格），按内容加宽显示当前格式色值（不裁切），按 `C` 复制；取色时用 `[` / `]` 或滚轮切换格式（`Tab` 只用于窗口/控件识别）
- **标注**：矩形、椭圆、箭头、画笔、文字、马赛克、序号；8 种颜色、3 档粗细 / 字号；撤销 / 重做
- **输出**：复制到剪贴板、保存为 PNG / JPG、钉在桌面
- **钉图**：拖动移动、滚轮缩放、`Ctrl`+滚轮调透明度、双击恢复原始大小、右键菜单复制 / 保存 / 关闭、`Esc` 关闭
- **钉图增强**：右键菜单旋转（`R` / `Shift+R`）、水平翻转（`H`）；托盘「钉图」子菜单可全部显示 / 隐藏 / 关闭
- **钉图再编辑**：钉图右键「编辑标注」或按 `E`，在原位置打开截图编辑器继续标注；完成后替换原钉图，取消则恢复
- **历史再编辑**：历史记录窗口「编辑标注」，在屏幕中央打开编辑器
- **延时截图**：托盘「延时截图」→ 3 / 5 / 10 秒
- **定时截图**：托盘「定时截图」→ 每 10 秒 / 30 秒 / 1 分钟 / 5 分钟自动保存全屏截图（多显示器各存一张）到保存位置，再选「停止定时截图」结束
- **长截图（滚动）**：框选后点工具条「长截图」或按 `L`（或托盘「长截图」后框选再按 Enter），自动向选区发送滚轮并按重叠区域拼接；识别静态固定顶栏 / 底栏，最终各保留一份；单击鼠标或 `Esc` 停止，到底自动停止。分段存储新增内容，预览只绘制最新部分，并显示帧数与像素尺寸。停止后支持复制（`Ctrl+C` / `Enter`）、保存（`Ctrl+S`）、另存为（`Ctrl+Shift+S`）和钉图（`F3`）；保存失败会保留窗口与长图，可重试或更换位置
- **截图历史**：每次复制 / 保存 / 钉图都会存入历史（默认保留 20 张）；托盘「最近截图」快速复制，「历史记录…」窗口可复制、钉图、打开文件夹、清空
- **文字识别（OCR）**：框选后点工具条「识别文字」或按 `O`，也可在钉图右键菜单中识别。默认使用 RapidOCR（PP-OCRv4 ONNX，适合中文和界面混排）；可在设置里改回 Windows 系统 OCR。模型约 16 MB，不随安装包附带，首次识别或在设置中下载。结果可编辑、保留换行、一键复制
- **识别并翻译**：工具条「识别文字」旁的「识别并翻译」，或全局热键 `Ctrl+Alt+T`（设置 → 常规可改，与截图 / 录屏冲突时会提示）。框选后一次完成识别，并按文字语言自动翻译。翻译未开启、或 AI 未配好且没有 MyMemory 兜底时，仍显示识别结果并说明原因。已是目标语言、无正文、以及本次识别里的译文缓存同样生效，不会多打请求
- **OCR 结果翻译**：识别面板可一键译成中文或英文（自动判断方向）。在设置中填写 OpenAI 兼容接口的地址、密钥和模型后优先走 AI；未配置或调用失败时使用 MyMemory（无需密钥）。密钥用 Windows DPAPI 加密保存在本机，不会写入日志。设置 → 截图中可关闭联网翻译；支持还原原文与失败提示
- **二维码 / 条码识别**：框选后点「识别二维码」或按 `Q`（钉图右键菜单同样可用）；支持 QR、Data Matrix、PDF417、EAN、Code128 等；识别到网址时可确认后在浏览器打开
- **取色器**：托盘「取色」，在多显示器上用放大镜定位，点击或 `C` 复制颜色；`[` / `]` 或滚轮切换格式（不占用 `Tab`）。支持 `#4F6BFF` / `#4f6bff` / `#4F6BFFFF`、`rgb(...)` / `rgba(...)`、`79, 107, 255`、`hsl(...)` / `hsv(...)`、Windows `0xBBGGRR`；默认格式可在设置里改
- **录屏 / GIF**：全局热键 `Ctrl+Alt+R`（可改）开始框选录制 MP4，录制中再按一次停止保存；或托盘「录屏」→「录制 MP4 / GIF」。框选后按 Enter 开始；悬浮迷你控制条可暂停 / 继续 / 停止并显示计时，控制条和虚线框不会被录进画面；`Esc` 停止；帧率 10 / 15 / 30 可设，可选录制鼠标指针与麦克风（仅 MP4）；优先使用 Desktop Duplication（DXGI）抓帧，不可用时回退 GDI；GIF 在后台线程编码并可用抖动减少色带；文件保存到设置的保存位置
- **主题**：浅色 / 深色 / 跟随系统（实时响应系统切换），主色保持靛蓝 `#4F6BFF`
- **多语言**：简体中文 / English / 跟随系统，在设置中切换后立即生效（托盘菜单与历史窗口自动刷新）；Qt 自带对话框同步翻译
- **自动更新**：启动时自动检查（每天最多一次，可关闭），托盘「检查更新」手动检查；显示更新说明，下载安装包并按 SHA256SUMS 校验后经确认启动安装；免安装版改为打开发布页
- **上传**（默认关闭）：在设置 → 上传中启用，支持 SM.MS、Imgur 和自定义 HTTP（multipart，可配置地址、文件字段、请求头、JSON 链接路径）；截图工具条「上传」按钮（`U`）或钉图右键菜单上传，成功后链接自动复制；令牌 / 请求头使用 Windows DPAPI 加密保存，不以明文落盘
- **设置**：左侧导航分为 常规 / 截图 / 标注 / 录屏 / 上传 / 外观与语言 / 关于与更新；分组卡片，每项附一行说明；开关、分段选择、快捷键录制框（点击后直接按组合键，冲突即时提示）、保存位置带“打开文件夹”、文件名实时预览、上传只显示所选服务需要的参数；修改即时生效（无“确定 / 取消”），输入有误时就地标红提示；窗口可缩放并记住大小；关于页含版本、检查更新、GitHub 与日志文件夹

## 快捷键（截图中）

| 按键 | 作用 |
|---|---|
| `Enter` / 双击选区 | 执行默认动作（默认复制，可在设置中改） |
| `Ctrl+C` | 复制到剪贴板并退出 |
| `L` | 对选区进行滚动长截图 |
| `O` | 识别选区文字（OCR） |
| `Ctrl+Alt+T` | 识别选区文字并自动翻译（全局热键，可在设置中修改；未框选时会提示） |
| `Q` | 识别选区二维码 / 条码 |
| `Ctrl+S` | 快速保存到设置的文件夹（不弹窗） |
| `Ctrl+Shift+S` | 另存为… |
| `F3` | 钉在桌面 |
| `U` | 上传并复制链接（需在设置中启用） |
| 钉图上 `E` | 编辑标注 |
| `Esc` | 退出截图 |
| 右键 | 退出截图（编辑文字时为取消输入）；想重新框选，可在无标注时于选区外直接拖动 |
| `R` `E` `A` `P` `T` `M` `N` | 矩形 / 椭圆 / 箭头 / 画笔 / 文字 / 马赛克 / 序号 |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 重做 |
| `Shift` + 拖动 | 画正方形 / 正圆 |
| `C` | 复制光标处颜色（当前取色格式） |
| `Tab` | 截图时切换窗口 / 窗口+控件识别（取色不占用） |
| `[` / `]` 或滚轮 | 取色 / 放大镜时切换颜色格式 |
| 文字工具中 `Enter` / `Ctrl+Enter` | 换行 / 完成输入（点击别处也会完成） |
| 方向键 | 光标微移 1 像素 |

## 文字识别模型（RapidOCR）

默认引擎是 RapidOCR，模型来自 [RapidAI/RapidOCR](https://github.com/RapidAI/RapidOCR) 的 PP-OCRv4 mobile（Apache-2.0），不放进仓库。首次点「识别文字」，或在 **设置 → 截图 → 文字识别** 里点「下载模型」，会下载到下面第一个可写目录（也可用环境变量 `QINGYING_OCR_MODELS` 指定）：

1. 程序所在目录的 `models`
2. `%LOCALAPPDATA%\QingYing\QingYing\models`

需要这四个文件（SHA256 与 RapidOCR v3.10.0 发布一致）：

| 文件 | SHA256 |
|---|---|
| `ch_PP-OCRv4_det_mobile.onnx` | `d2a7720d45a54257208b1e13e36a8479894cb74155a5efe29462512d42f49da9` |
| `ch_PP-OCRv4_rec_mobile.onnx` | `48fc40f24f6d2a207a2b1091d3437eb3cc3eb6b676dc3ef9c37384005483683b` |
| `ch_ppocr_mobile_v2.0_cls_mobile.onnx` | `e47acedf663230f8863ff1ab0e64dd2d82b838fceb5957146dab185a89d6215c` |
| `ppocr_keys_v1.txt` | `28b2362ad4ab2dc38769aa72feb535e3a9ddb3fd2a7585a05920e6393b1dc7f7` |

手动下载（ModelScope）：

```powershell
$dir = "$env:LOCALAPPDATA\QingYing\QingYing\models"
New-Item -ItemType Directory -Force $dir | Out-Null
$base = "https://www.modelscope.cn/models/RapidAI/RapidOCR/resolve/v3.10.0"
Invoke-WebRequest "$base/onnx/PP-OCRv4/det/ch_PP-OCRv4_det_mobile.onnx" -OutFile "$dir\ch_PP-OCRv4_det_mobile.onnx"
Invoke-WebRequest "$base/onnx/PP-OCRv4/rec/ch_PP-OCRv4_rec_mobile.onnx" -OutFile "$dir\ch_PP-OCRv4_rec_mobile.onnx"
Invoke-WebRequest "$base/onnx/PP-OCRv4/cls/ch_ppocr_mobile_v2.0_cls_mobile.onnx" -OutFile "$dir\ch_ppocr_mobile_v2.0_cls_mobile.onnx"
Invoke-WebRequest "$base/paddle/PP-OCRv4/rec/ch_PP-OCRv4_rec_mobile/ppocr_keys_v1.txt" -OutFile "$dir\ppocr_keys_v1.txt"
```

也接受旧文件名 `ch_PP-OCRv4_det_infer.onnx`、`ch_PP-OCRv4_rec_infer.onnx`、`ch_ppocr_mobile_v2.0_cls_infer.onnx`。模型缺失、校验失败或当前构建没有 ONNX Runtime 时，自动改用系统 OCR。

推理库是 [ONNX Runtime 1.23.2](https://github.com/microsoft/onnxruntime/releases/tag/v1.23.2) Windows x64，CMake 配置时从 GitHub 下载并复制到 exe 旁边，不提交进仓库。没有网络时可 `-DQINGYING_WITH_ONNX=OFF`，或用 `-DONNXRUNTIME_ROOT=` 指向已解压的 SDK。

## 系统 OCR 语言包

设置里把引擎改成「系统 OCR」后，使用 Windows.Media.Ocr，不需要上面的模型。若提示没有可用的 OCR 语言包：打开 **设置 → 时间和语言 → 语言和区域**，添加 **中文(简体)**，并在语言选项里安装 **光学字符识别**。没有中文语言包时会退回系统当前语言。

## AI 翻译

**设置 → 截图 → 翻译** 打开「使用 AI 翻译」，填写：

- 接口地址：OpenAI 兼容根路径，例如 `https://api.openai.com/v1`（程序会请求 `/chat/completions`）
- API 密钥：只在设置页输入。程序用 Windows DPAPI 加密后写入本机 QSettings，不明文落盘，也不写入日志
- 模型名：由服务商提供，例如 `gpt-4o-mini`

不要把密钥写进仓库、截图或问题描述。未填完整，或接口失败时，若「失败时用 MyMemory」开着（默认开），会改用 MyMemory。关掉「联网翻译」后识别面板不再显示翻译按钮。

## 已知限制

- 选区暂不支持跨显示器：每块屏幕独立冻结并按各自 DPI 显示。若把各屏拼成一张物理像素虚拟桌面，混合 DPI（如 100% + 150%）下遮罩在其中一块屏幕上必然被缩放，放大镜与标注无法做到像素精确，因此保留按屏选区。需要跨屏内容时可分别截图，或使用定时截图保存每块屏幕。
- 录屏：Desktop Duplication 不支持旋转屏、跨显示器选区及部分远程桌面场景，这些情况自动使用 GDI（CPU 占用较高）；GIF 最多 256 色，大面积渐变即使开启抖动也会有颗粒感，适合短小片段；暂不录音。
- 钉图 / 历史再编辑时，比屏幕大的图片会先缩小到屏幕大小。
- 长截图依赖目标程序响应鼠标滚轮；支持保持静止且不透明的固定顶栏 / 底栏，动态或半透明悬浮栏、动画、视差和大量重复内容仍可能无法对齐。匹配失败时先等待当前画面稳定，连续失败后停止并保留已捕获内容。输出上限为 30,000 像素高或 64 Mi 像素（RGB32 图像约 256 MiB）；达到上限时保留当前结果，不再追加。

## 诊断

日志与崩溃转储位于 `%LOCALAPPDATA%\QingYing\QingYing\logs`：`qingying.log` 记录启动、退出原因与 Qt 警告；未处理异常会生成 `crash_*.dmp`（保留最近 5 个）。反馈问题时可附上这两类文件。

## 下载

**国内下载（推荐）：**

- 安装包直链：[QingYing-0.6.6-windows-x64-setup.exe](https://gitee.com/liner123/qing-ying/releases/download/v0.6.6/QingYing-0.6.6-windows-x64-setup.exe)
- 免安装 ZIP：[QingYing-0.6.6-windows-x64-portable.zip](https://gitee.com/liner123/qing-ying/releases/download/v0.6.6/QingYing-0.6.6-windows-x64-portable.zip)
- 校验和：[SHA256SUMS-0.6.6-x64.txt](https://gitee.com/liner123/qing-ying/releases/download/v0.6.6/SHA256SUMS-0.6.6-x64.txt)
- 全部版本：[Gitee Releases](https://gitee.com/liner123/qing-ying/releases) · [v0.6.6](https://gitee.com/liner123/qing-ying/releases/tag/v0.6.6)

GitHub：[Releases](https://github.com/nullTerminatedCat/QingYing/releases)（应用内「检查更新」仍走 GitHub）。

## 构建

依赖：Visual Studio 2022（MSVC x64，含 Windows 10/11 SDK 与 C++/WinRT 头文件）、Qt 6.7+（开发时使用 Qt 6.9.1 msvc2022_64，需包含 qttools 以提供 Linguist 工具）、CMake 3.21+、Ninja。

```powershell
# 使用默认 Qt 路径 F:/Qt/6.9.1/msvc2022_64
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
# 或指定 Qt 路径
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -QtDir C:/Qt/6.9.1/msvc2022_64
```

GitHub Actions 会在每次推送时于 windows-latest 构建并上传 `QingYing-windows-x64` 产物。

首次配置时 CMake 会通过 FetchContent 从 GitHub 下载 ZXing-cpp v2.2.1，并下载 ONNX Runtime 1.23.2（Windows x64）供 RapidOCR 链接。单元测试不需要这些模型。

单元测试：`ctest --test-dir build`（QtTest，覆盖拼接位移、文件名规则、取色格式、版本比较、SHA256SUMS 解析、JSON 路径、OCR 检测框 / CTC / 翻译请求解析），CI 每次推送都会运行。模型与 API 密钥不参与测试。

长图性能基准：`build/qingying_scroll_bench.exe`（需要 Qt DLL 在可执行文件目录或 PATH 中）。对比 v0.5.2 的整图拷贝 / 缩放循环与分段缓冲 / 局部预览，包含最终合成并验证像素一致，不包含屏幕抓取、滚动等待与位移匹配；耗时受机器负载影响。

打包（需要 Inno Setup 6）：`powershell -ExecutionPolicy Bypass -File scripts/package.ps1`，产物在 `dist/`。推送 `v*` 标签时 GitHub Actions 会自动构建并发布 Release。

产物：`build/QingYing.exe`（构建后自动运行 windeployqt 部署 Qt 运行库）。

## 项目结构

```
src/
  main.cpp            入口、单实例
  App.*               托盘、热键、截图会话
  HotkeyManager.*     RegisterHotKey 全局热键
  ScreenGrabber.*     GDI 物理像素抓屏
  WindowDetector.*    EnumWindows 窗口 / 控件识别
  CaptureOverlay.*    截图遮罩、选区、放大镜、标注
  Annotation.*        标注数据与绘制
  ToolBars.*          悬浮工具条与样式条
  PinWindow.*         钉图窗口与钉图管理
  ScrollCapture.*     滚动长截图与拼接
  ScrollStitcher.*    固定栏匹配、分段长图缓冲与局部预览
  History.* HistoryWindow.*  截图历史
  Ocr.* OcrPost.* OcrModels.* RapidOcr.* OcrWin.*  RapidOCR（ONNX）与系统 OCR
  QrDecoder.* Recognize.* ResultDialog.* Translator.* TranslateText.*  二维码 / 翻译与结果面板
  Recorder.* RecordSession.*  录屏（MP4 / GIF）与控制条
third_party/gif/      gif.h（公有领域）
  SettingsDialog.*    设置窗口（侧栏导航、即时生效）
  SettingsWidgets.*   开关、分段控件、快捷键录制框
  Settings.*          QSettings 持久化
  Style.* Icons.*     设计规范颜色与线性图标
  Theme.*             浅色 / 深色主题
  Updater.*           检查更新、下载与 SHA-256 校验
  Uploader.* Secret.* 上传插件与 DPAPI 密钥保存
i18n/                 Qt Linguist 翻译（源文为简体中文，en_US 为英文）
docs/design/          UI 设计稿与规范
```

## 许可证

[MIT](LICENSE)。第三方组件见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

---

## English

QingYing (青影) is a simple, good-looking screenshot tool for Windows: an indigo accent (`#4F6BFF`), a floating rounded toolbar, and secondary options only when you need them.

**Features**
- Tray app with global hotkeys (capture `Ctrl+Alt+A`, record `Ctrl+Alt+R`, recognize-and-translate `Ctrl+Alt+T`, all configurable with conflict detection)
- Region capture that freezes every monitor at native DPI, detects windows and controls (`Tab`), and shows a pixel magnifier (`[`/`]` or wheel cycles color formats while picking)
- Annotations: rectangle, ellipse, arrow, pen, text, mosaic, and numbered steps, with undo and redo
- Output: copy, quick save (`Ctrl+S`), save as, and pin to desktop (`F3`); pins can be zoomed, made translucent, rotated, and flipped
- Scrolling capture (`L`) with static fixed header/footer handling, incremental image buffering and a live size/frame counter; copy (`Ctrl+C` / Enter), save (`Ctrl+S`), save as (`Ctrl+Shift+S`) or pin (`F3`) after capture. Failed saves keep the image available for retry. Delayed capture and capture history are also supported.
- Offline OCR (`O`): RapidOCR (PP-OCRv4 ONNX, models downloaded on first use, about 16 MB) with Windows.Media.Ocr as fallback. Optional translation via an OpenAI-compatible API (base URL, key and model in Settings; key stored with DPAPI, never logged) or MyMemory when AI is not configured. The toolbar action “Recognize and translate”, or the global hotkey `Ctrl+Alt+T`, runs OCR and then translates in one step. QR / barcode scanning (`Q`, via ZXing-cpp)
- Color picker, plus MP4 / GIF screen recording (Desktop Duplication with GDI fallback)
- Re-annotate a pin (`E`) or a history item; default tool and color; timed full-screen capture
- Light, dark, or follow-system themes
- Settings with sidebar navigation, toggle switches, a hotkey recorder, live file-name preview, and instant apply (no OK / Cancel)
- Simplified Chinese or English UI, switchable at runtime
- Auto-update with release notes and SHA-256 verification; the portable build opens the release page instead
- Optional upload (off by default): SM.MS, Imgur, or custom HTTP multipart. The link is copied to the clipboard, and secrets are encrypted with Windows DPAPI.

**Download (China, recommended):**
- Setup: [QingYing-0.6.6-windows-x64-setup.exe](https://gitee.com/liner123/qing-ying/releases/download/v0.6.6/QingYing-0.6.6-windows-x64-setup.exe)
- Portable: [QingYing-0.6.6-windows-x64-portable.zip](https://gitee.com/liner123/qing-ying/releases/download/v0.6.6/QingYing-0.6.6-windows-x64-portable.zip)
- All releases: [Gitee Releases](https://gitee.com/liner123/qing-ying/releases)

GitHub: [Releases](https://github.com/nullTerminatedCat/QingYing/releases) (in-app auto-update still uses GitHub).

**Build:** you need Visual Studio 2022 (MSVC x64, Windows SDK), Qt 6.7+ with qttools, CMake 3.21+, and Ninja.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -QtDir C:/Qt/6.9.1/msvc2022_64
```

**Tests:** `ctest --test-dir build` runs the QtTest unit tests in `tests/` (stitching, filename patterns, color formats, semver, SHA256SUMS, JSON paths); CI runs them on every push.

**Translations:** the source strings are Simplified Chinese. English lives in `i18n/qingying_en_US.ts`. After changing strings, run the `update_translations` target and edit the file with Qt Linguist.

**Diagnostics:** logs and crash minidumps are written to `%LOCALAPPDATA%\QingYing\QingYing\logs`.

License: [MIT](LICENSE).
