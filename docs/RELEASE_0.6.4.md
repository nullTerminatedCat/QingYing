# 青影 v0.6.4

取色器支持更多 RGB 颜色格式，取色时可用 Tab 一键切换并复制。

## 新特性

- **9 种取色格式**：`#RRGGBB` / `#rrggbb` / `#RRGGBBAA`、`rgb(...)` / `rgba(...)`、`R, G, B`、`hsl(...)` / `hsv(...)`、Windows `0xBBGGRR`
- **取色时 Tab 切换格式**（Shift+Tab 反向）；设置里可改默认格式；切换后立即生效并记住
- **复制反馈**：放大镜显示「已复制」约 0.9 秒；独立取色模式稍作停留再退出，避免像没反应
- 截图放大镜与托盘「取色」共用同一套格式逻辑

## 验证

- 本地 Release x64 零警告编译；QtTest 全绿；冒烟脚本通过
- 建议手测：托盘取色 → Tab 切换各格式 → C 复制；设置改默认格式后截图态按 C

## 产物

- QingYing-0.6.4-windows-x64-setup.exe：Windows x64 安装包
- QingYing-0.6.4-windows-x64-portable.zip：免安装版，解压即运行 QingYing.exe
- SHA256SUMS-0.6.4-x64.txt：上述两个文件的 SHA-256 校验值
