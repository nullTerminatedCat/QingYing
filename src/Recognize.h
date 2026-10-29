#pragma once
#include "AskText.h"

#include <QImage>
#include <QRect>

// 识别入口：弹出结果面板。anchor 非空时，把面板放在该矩形旁边（钉图秒答 / 改文案）。
namespace Recognize {
void ocr(const QImage& image, const QRect& anchor = {});
// 识别成功且有文字时，按 suggestTarget 走现有翻译（含跳过与缓存）。翻译未开或调用失败时仍保留识别结果并提示。
void ocrAndTranslate(const QImage& image, const QRect& anchor = {});
// 识别后聚焦提问框。没有文字时不请求。
void ocrAndAsk(const QImage& image, const QRect& anchor = {});
// 识别成功且有文字时按预设改写。没有文字时不请求。
void ocrAndRewrite(const QImage& image, AskText::Mode mode, const QRect& anchor = {});
void qr(const QImage& image);
}
