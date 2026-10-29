# 青影 QingYing 0.7.0

本版本把文字识别换成更适合中文和界面混排的 RapidOCR，并支持用自己的 OpenAI 兼容接口翻译识别结果。MyMemory 仍作为不需要密钥的兜底。

## 文字识别

- 默认引擎为 RapidOCR（PP-OCRv4 mobile，ONNX Runtime）。设置 → 截图 → 文字识别可改回「系统 OCR」。
- 模型约 16 MB，不在安装包和仓库里。第一次识别，或在设置里点「下载模型」，会下载到程序目录的 `models`，写不进去时改到 `%LOCALAPPDATA%\QingYing\QingYing\models`。也可以自己放入这四个文件：`ch_PP-OCRv4_det_mobile.onnx`、`ch_PP-OCRv4_rec_mobile.onnx`、`ch_ppocr_mobile_v2.0_cls_mobile.onnx`、`ppocr_keys_v1.txt`。地址和 SHA256 见 README。
- 模型缺失、校验失败，或这次构建没有链接 ONNX Runtime 时，自动改用 Windows 系统 OCR，并在结果面板说明。

## 翻译

- 识别面板仍是：可编辑文本、复制、翻译（自动方向）、译成中文、译成英文、还原原文。
- 设置里打开「使用 AI 翻译」后，填写接口地址（到 `/v1`）、API 密钥和模型名。程序请求 `{地址}/chat/completions`。
- 密钥只在设置页输入，用 Windows DPAPI 加密保存，不会写入日志。请不要把密钥发到仓库、截图或问题描述里。
- 「失败时用 MyMemory」默认开着：AI 没填完整或调用失败时改用 MyMemory。关掉「联网翻译」后不再显示翻译按钮。

## 构建

- CMake 配置时会下载 ONNX Runtime 1.23.2 Windows x64，并把 `onnxruntime.dll` 复制到 exe 旁边。不需要 RapidOCR 时使用 `-DQINGYING_WITH_ONNX=OFF`。
- 已有 SDK 时使用 `-DONNXRUNTIME_ROOT=<解压目录>`。
- 单元测试增加检测框、CTC、文本行合并和 Chat Completions 地址 / 响应解析，不需要模型和密钥。

## 怎么试

1. Windows 上按 README 构建。第一次 OCR 时看设置页模型是否变成「已就绪」，或看结果是否来自 RapidOCR。
2. 用一段中英混排的界面截图对比「RapidOCR」和「系统 OCR」。
3. AI 翻译：在设置中粘贴你自己的接口地址、密钥和模型，对识别结果点「译成英文」。失败时应能落到 MyMemory（若该开关开着）。
