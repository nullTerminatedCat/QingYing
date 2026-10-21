# 青影 QingYing

[English](#english) · 简体中文

简洁、好看的 Windows 桌面截图工具。靛蓝主色、悬浮圆角工具条，只在需要时出现二级选项。

> 当前版本：0.4.0（第一至四期：截图、效率、智能、打磨）。完整规划见 [PRODUCT_PLAN.md](PRODUCT_PLAN.md)，界面设计稿见 [docs/design](docs/design)。

## 功能

- **托盘常驻**：单击托盘图标截图；右键菜单包含截图、设置、开机自启、关于、退出
- **全局热键**：默认 `Ctrl+Alt+A`，可在设置中修改，被占用时会提示
- **区域截图**
  - 冻结所有显示器，每块屏幕按各自 DPI 像素精确截取
  - 自动识别光标下的窗口和控件，单击即可选中；`Tab` 切换"窗口 / 窗口+控件"识别
  - 拖拽框选，8 个手柄调整大小，拖动选区内部移动
  - 圆形放大镜（13×13 像素网格），显示 HEX 与 RGB，按 `C` 复制 HEX
- **标注**：矩形、椭圆、箭头、画笔、文字、马赛克、序号；8 种颜色、3 档粗细 / 字号；撤销 / 重做
- **输出**：复制到剪贴板、保存为 PNG / JPG、钉在桌面
- **钉图**：拖动移动、滚轮缩放、`Ctrl`+滚轮调透明度、双击恢复原始大小、右键菜单复制 / 保存 / 关闭、`Esc` 关闭
- **钉图增强**：右键菜单旋转（`R` / `Shift+R`）、水平翻转（`H`）；托盘「钉图」子菜单可全部显示 / 隐藏 / 关闭
- **延时截图**：托盘「延时截图」→ 3 / 5 / 10 秒
- **长截图（滚动）**：框选后点工具条「长截图」或按 `L`（或托盘「长截图」后框选再按 Enter），自动向选区发送滚轮并按重叠区域拼接；单击鼠标或 `Esc` 停止，到底自动停止，然后选择复制 / 保存 / 钉图
- **截图历史**：每次复制 / 保存 / 钉图都会存入历史（默认保留 20 张）；托盘「最近截图」快速复制，「历史记录…」窗口可复制、钉图、打开文件夹、清空
- **文字识别（OCR）**：框选后点工具条「识别文字」或按 `O`，也可在钉图右键菜单中识别；基于 Windows 自带的离线 OCR（Windows.Media.Ocr），优先简体中文，结果可编辑、保留换行、一键复制
- **二维码 / 条码识别**：框选后点「识别二维码」或按 `Q`（钉图右键菜单同样可用）；支持 QR、Data Matrix、PDF417、EAN、Code128 等；识别到网址时可确认后在浏览器打开
- **取色器**：托盘「取色」，在冻结屏幕上用放大镜定位，单击或按 `C` 复制颜色；格式可选 `#4F6BFF` / `#4f6bff` / `rgb(79, 107, 255)` / `79, 107, 255`
- **录屏 / GIF**：托盘「录屏」→「录制 MP4」或「录制 GIF」，框选区域后按 Enter 开始；悬浮迷你控制条可暂停 / 继续 / 停止并显示计时，控制条和虚线框不会被录进画面；`Esc` 停止；帧率 10 / 15 / 30 可设，可选录制鼠标指针；文件保存到设置的保存位置
- **主题**：浅色 / 深色 / 跟随系统（实时响应系统切换），主色保持靛蓝 `#4F6BFF`
- **多语言**：简体中文 / English / 跟随系统，在设置中切换，重启后生效
- **自动更新**：启动时自动检查（每天最多一次，可关闭），托盘「检查更新」手动检查；显示更新说明，下载安装包并按 SHA256SUMS 校验后经确认启动安装；免安装版改为打开发布页
- **上传**（默认关闭）：在设置 → 上传中启用，支持 SM.MS、Imgur 和自定义 HTTP（multipart，可配置地址、文件字段、请求头、JSON 链接路径）；截图工具条「上传」按钮（`U`）或钉图右键菜单上传，成功后链接自动复制；令牌 / 请求头使用 Windows DPAPI 加密保存，不以明文落盘
- **设置**：热键、保存位置、文件名规则（`{}` 内为日期格式）、默认格式、完成时默认动作（复制 / 保存 / 钉图）、历史保留数量、开机自启

## 快捷键（截图中）

| 按键 | 作用 |
|---|---|
| `Enter` / 双击选区 | 执行默认动作（默认复制，可在设置中改） |
| `Ctrl+C` | 复制到剪贴板并退出 |
| `L` | 对选区进行滚动长截图 |
| `O` | 识别选区文字（OCR） |
| `Q` | 识别选区二维码 / 条码 |
| `Ctrl+S` | 快速保存到设置的文件夹（不弹窗） |
| `Ctrl+Shift+S` | 另存为… |
| `F3` | 钉在桌面 |
| `U` | 上传并复制链接（需在设置中启用） |
| `Esc` | 退出截图 |
| 右键 | 退出截图（编辑文字时为取消输入）；想重新框选，可在无标注时于选区外直接拖动 |
| `R` `E` `A` `P` `T` `M` `N` | 矩形 / 椭圆 / 箭头 / 画笔 / 文字 / 马赛克 / 序号 |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 重做 |
| `Shift` + 拖动 | 画正方形 / 正圆 |
| `C` | 复制光标处颜色 HEX |
| `Tab` | 切换窗口 / 控件识别 |
| 文字工具中 `Enter` / `Ctrl+Enter` | 换行 / 完成输入（点击别处也会完成） |
| 方向键 | 光标微移 1 像素 |

## OCR 语言包

文字识别使用 Windows 系统自带的 OCR 引擎，不随软件附带模型。若提示"没有可用的 OCR 语言包"：
打开 **设置 → 时间和语言 → 语言和区域**，添加 **中文(简体)**（或在已有语言的"语言选项"中安装 **光学字符识别**），完成后重试即可。
没有中文语言包时会退回到系统当前语言（如英文），中文将无法识别。

## 已知限制

- 选区暂不支持跨显示器（每块屏幕独立冻结，以保证混合 DPI 下像素精确）。
- 录屏使用 GDI 抓帧（未使用 Desktop Duplication），高分辨率 / 30 fps 下 CPU 占用较高；GIF 编码较慢、文件较大，适合短小片段；暂不录音。
- 长截图依赖目标程序响应鼠标滚轮；带固定底栏、动画或视差效果的页面可能拼接不准。

## 下载

前往 [Releases](https://github.com/nullTerminatedCat/QingYing/releases) 下载安装包（setup.exe）或免安装版（portable.zip）。

## 构建

依赖：Visual Studio 2022（MSVC x64，含 Windows 10/11 SDK 与 C++/WinRT 头文件）、Qt 6.7+（开发时使用 Qt 6.9.1 msvc2022_64，需包含 qttools 以提供 Linguist 工具）、CMake 3.21+、Ninja。

```powershell
# 使用默认 Qt 路径 F:/Qt/6.9.1/msvc2022_64
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
# 或指定 Qt 路径
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -QtDir C:/Qt/6.9.1/msvc2022_64
```

GitHub Actions 会在每次推送时于 windows-latest 构建并上传 `QingYing-windows-x64` 产物。

首次配置时 CMake 会通过 FetchContent 从 GitHub 下载 ZXing-cpp v2.2.1。

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
  History.* HistoryWindow.*  截图历史
  Ocr.* QrDecoder.* Recognize.* ResultDialog.*  OCR / 二维码识别与结果面板
  Recorder.* RecordSession.*  录屏（MP4 / GIF）与控制条
third_party/gif/      gif.h（公有领域）
  SettingsDialog.*    设置窗口
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
- Tray app with a global hotkey (default `Ctrl+Alt+A`, configurable, with conflict detection)
- Region capture that freezes every monitor at native DPI, detects windows and controls (`Tab`), and shows a pixel magnifier
- Annotations: rectangle, ellipse, arrow, pen, text, mosaic, and numbered steps, with undo and redo
- Output: copy, quick save (`Ctrl+S`), save as, and pin to desktop (`F3`); pins can be zoomed, made translucent, rotated, and flipped
- Scrolling capture (`L`), delayed capture, and capture history
- Offline OCR (`O`, via Windows.Media.Ocr) and QR / barcode scanning (`Q`, via ZXing-cpp)
- Color picker, plus MP4 / GIF screen recording
- Light, dark, or follow-system themes
- Simplified Chinese or English UI; the language setting takes effect after a restart
- Auto-update with release notes and SHA-256 verification; the portable build opens the release page instead
- Optional upload (off by default): SM.MS, Imgur, or custom HTTP multipart. The link is copied to the clipboard, and secrets are encrypted with Windows DPAPI.

**Download:** [Releases](https://github.com/nullTerminatedCat/QingYing/releases) provides `setup.exe` or `portable.zip`.

**Build:** you need Visual Studio 2022 (MSVC x64, Windows SDK), Qt 6.7+ with qttools, CMake 3.21+, and Ninja.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -QtDir C:/Qt/6.9.1/msvc2022_64
```

**Translations:** the source strings are Simplified Chinese. English lives in `i18n/qingying_en_US.ts`. After changing strings, run the `update_translations` target and edit the file with Qt Linguist.

License: [MIT](LICENSE).
