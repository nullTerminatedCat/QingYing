# 第三方组件声明 / Third-Party Notices

青影 QingYing 使用了以下第三方组件，感谢它们的作者。

| 组件 | 用途 | 许可证 | 来源 |
|---|---|---|---|
| Qt 6 | 界面框架 | LGPL-3.0（动态链接） | https://www.qt.io |
| ZXing-cpp v2.2.1 | 二维码 / 条码识别 | Apache-2.0 | https://github.com/zxing-cpp/zxing-cpp |
| gif.h（Charlie Tangora） | GIF 编码 | Public Domain | https://github.com/charlietangora/gif-h |
| Windows.Media.Ocr / Media Foundation | 系统 OCR（可选）/ MP4 编码 | Windows 系统组件 | Microsoft |
| ONNX Runtime 1.23.2 | RapidOCR 推理 | MIT | https://github.com/microsoft/onnxruntime |
| RapidOCR / PaddleOCR PP-OCRv4 模型 | 中文检测、识别、方向分类与字典 | Apache-2.0 | https://github.com/RapidAI/RapidOCR 、 https://github.com/PaddlePaddle/PaddleOCR |

## ZXing-cpp

Copyright 2016 Nu-book Inc. and contributors.
Licensed under the Apache License, Version 2.0 (the "License"); you may not use this
file except in compliance with the License. You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under
the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied. See the License for the specific language governing
permissions and limitations under the License.

## Qt

Qt is licensed under the GNU Lesser General Public License v3. QingYing links to Qt
dynamically; the Qt DLLs shipped with the application can be replaced by the user.
Source code for Qt is available at https://download.qt.io.

## gif.h

gif.h by Charlie Tangora is released into the public domain.

## ONNX Runtime

QingYing links the official ONNX Runtime 1.23.2 Windows x64 build (MIT License) for local RapidOCR.
Copyright © Microsoft Corporation. Source and license: https://github.com/microsoft/onnxruntime

The DLL is downloaded by CMake at configure time and copied next to the executable. It is not stored in this repository.

## RapidOCR / PaddleOCR models

Text detection, recognition, angle classification and the character dictionary are the PP-OCRv4 mobile ONNX models published by RapidAI (RapidOCR) and derived from PaddleOCR. Both projects are licensed under the Apache License 2.0.

These files are not shipped in the repository. QingYing downloads them on first use from ModelScope (`RapidAI/RapidOCR` v3.10.0) or the user can place them in the models folder. See README.

## MyMemory Translation API

OCR 结果在未配置 AI 翻译（或 AI 调用失败且允许兜底）时，调用 [MyMemory](https://mymemory.translated.net/) 的公开 HTTP 接口（`api.mymemory.translated.net`），无需 API Key。该服务有免费额度限制；可在设置 → 截图 →「联网翻译」中关闭。请求只发送识别文本，不上传截图。

## AI 翻译接口

若用户在设置中启用 AI 翻译，程序会把待译文本发到用户填写的 OpenAI 兼容地址。API 密钥只通过设置界面输入，并用 Windows DPAPI 加密后保存在本机，不会写入日志、仓库或诊断文件。青影不附带任何密钥。
