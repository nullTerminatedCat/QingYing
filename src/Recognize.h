#pragma once
#include <QImage>

// 识别入口：弹出结果面板
namespace Recognize {
void ocr(const QImage& image);
void qr(const QImage& image);
}
