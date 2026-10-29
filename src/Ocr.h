#pragma once
#include <QImage>
#include <QString>
#include <functional>

namespace Ocr {
struct Result {
    bool ok = false;
    QString text;     // 保留换行
    QString language; // 识别引擎 / 语言
    QString error;
    bool fellBack = false; // RapidOCR 不可用，结果来自系统 OCR
    QString note;          // 回退原因，可拼进状态栏
};
// 按设置选择 RapidOCR 或系统 OCR。模型缺失时会下载并在失败时回退系统 OCR。
// 阻塞，请在工作线程调用。progress 在同一线程回调（可为下载进度）。
Result recognize(const QImage& image, const std::function<void(const QString&)>& progress = {});
}
