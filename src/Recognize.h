#pragma once
#include <QImage>

// 识别入口：弹出结果面板
namespace Recognize {
void ocr(const QImage& image);
// 识别成功且有文字时，按 suggestTarget 走现有翻译（含跳过与缓存）。翻译未开或调用失败时仍保留识别结果并提示。
void ocrAndTranslate(const QImage& image);
void qr(const QImage& image);
}
