#pragma once
#include <QImage>
#include <QList>
#include <QString>

namespace QrDecoder {
struct Code {
    QString format;
    QString text;
};
// 识别图中所有二维码 / 条码（ZXing-cpp）
QList<Code> decode(const QImage& image);
}
