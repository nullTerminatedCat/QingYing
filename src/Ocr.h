#pragma once
#include <QImage>
#include <QString>

namespace Ocr {
struct Result {
    bool ok = false;
    QString text;     // 保留换行
    QString language; // 识别语言显示名
    QString error;
};
// 使用 Windows.Media.Ocr 离线识别；阻塞，请在工作线程调用
Result recognize(const QImage& image);
}
