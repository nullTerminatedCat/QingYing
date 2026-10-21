# 青影 QingYing

简洁、好看的 Windows 桌面截图工具。靛蓝主色、悬浮圆角工具条，只在需要时出现二级选项。

> 当前版本：0.2.0（第一期 MVP + 第二期效率功能）。完整规划见 [PRODUCT_PLAN.md](PRODUCT_PLAN.md)，界面设计稿见 [docs/design](docs/design)。

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
- **设置**：热键、保存位置、文件名规则（`{}` 内为日期格式）、默认格式、完成时默认动作（复制 / 保存 / 钉图）、历史保留数量、开机自启

## 快捷键（截图中）

| 按键 | 作用 |
|---|---|
| `Enter` / 双击选区 | 执行默认动作（默认复制，可在设置中改） |
| `Ctrl+C` | 复制到剪贴板并退出 |
| `L` | 对选区进行滚动长截图 |
| `Ctrl+S` | 快速保存到设置的文件夹（不弹窗） |
| `Ctrl+Shift+S` | 另存为… |
| `F3` | 钉在桌面 |
| `Esc` | 退出截图 |
| 右键 | 退出截图（编辑文字时为取消输入）；想重新框选，可在无标注时于选区外直接拖动 |
| `R` `E` `A` `P` `T` `M` `N` | 矩形 / 椭圆 / 箭头 / 画笔 / 文字 / 马赛克 / 序号 |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 重做 |
| `Shift` + 拖动 | 画正方形 / 正圆 |
| `C` | 复制光标处颜色 HEX |
| `Tab` | 切换窗口 / 控件识别 |
| 文字工具中 `Enter` / `Ctrl+Enter` | 换行 / 完成输入（点击别处也会完成） |
| 方向键 | 光标微移 1 像素 |

## 已知限制

- 选区暂不支持跨显示器（每块屏幕独立冻结，以保证混合 DPI 下像素精确）。
- 长截图依赖目标程序响应鼠标滚轮；带固定底栏、动画或视差效果的页面可能拼接不准。

## 构建

依赖：Visual Studio 2022（MSVC x64）、Qt 6.7+（开发时使用 Qt 6.9.1 msvc2022_64）、CMake 3.21+、Ninja。

```powershell
# 使用默认 Qt 路径 F:/Qt/6.9.1/msvc2022_64
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
# 或指定 Qt 路径
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -QtDir C:/Qt/6.9.1/msvc2022_64
```

GitHub Actions 会在每次推送时于 windows-latest 构建并上传 `QingYing-windows-x64` 产物。

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
  SettingsDialog.*    设置窗口
  Settings.*          QSettings 持久化
  Style.* Icons.*     设计规范颜色与线性图标
docs/design/          UI 设计稿与规范
```

## 许可证

[MIT](LICENSE)
